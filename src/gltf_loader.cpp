#include "gltf_loader.h"
#include "utils.h"

GLBData load_glb(const std::filesystem::path& glb_path) {
	using namespace fastgltf;

	Parser parser;
	GltfDataBuffer data;
	data.loadFromFile(glb_path);
	Expected<Asset> asset = parser.loadGltfBinary(&data, glb_path.parent_path());

	//Pull out all materials in the asset before walking the primitives of the mesh
	std::vector<GLBMaterial> materials;
	materials.reserve(asset->materials.size());
	for (fastgltf::Material& mat : asset->materials) {
		PBRData& pbr = mat.pbrData;

		GLBMaterial material;
		material.base_color[0] = pbr.baseColorFactor[0];
		material.base_color[1] = pbr.baseColorFactor[1];
		material.base_color[2] = pbr.baseColorFactor[2];
		material.base_color[3] = pbr.baseColorFactor[3];
		material.color_image_format = VK_FORMAT_R8G8B8A8_SRGB;
		
		//Load base color texture
		if (pbr.baseColorTexture.has_value()) {
			TextureInfo& info = pbr.baseColorTexture.value();
			Texture& tex = asset->textures[info.textureIndex];
			Image& im = asset->images[tex.imageIndex.value()];

			const sources::BufferView* data_ptr = std::get_if<sources::BufferView>(&im.data);
			PRORENDER_ASSERT(data_ptr != nullptr, true);
			PRORENDER_ASSERT(data_ptr->mimeType == MimeType::JPEG || data_ptr->mimeType == MimeType::PNG, true);

			fastgltf::BufferView& bv = asset->bufferViews[data_ptr->bufferViewIndex];
			fastgltf::Buffer& buffer = asset->buffers[bv.bufferIndex];

			const sources::ByteView* arr = std::get_if<sources::ByteView>(&buffer.data);
			PRORENDER_ASSERT(arr != nullptr, true);

			material.color_image_bytes.resize(bv.byteLength);
			memcpy(material.color_image_bytes.data(), arr->bytes.data() + bv.byteOffset, bv.byteLength);

			//printf("Loading image \"%s\"\n", im.name.c_str());
		}
		materials.push_back(material);
	}

	std::vector<GLBPrimitive> primitives;
	std::vector<uint16_t> prim_parents;
	for (Node& node : asset->nodes) {
		std::vector<float> positions;
		std::vector<float> colors;
		std::vector<float> uvs;
		std::vector<uint16_t> indices;

		if (node.meshIndex.has_value()) {
			size_t mesh_idx = node.meshIndex.value();
			Mesh& mesh = asset->meshes[mesh_idx];

			//Just loading the first primitive for now
			for (Primitive& prim : mesh.primitives) {
				bool has_color = false;
				for (auto& a : prim.attributes) {
					if (a.first == "COLOR_0") {
						has_color = true;
					}
				}

				//Loading vertex position data
				{
					uint64_t accessor_index = prim.findAttribute("POSITION")->second;
					Accessor& accessor = asset->accessors[accessor_index];
					positions.reserve(4 * accessor.count);
					auto iterator = fastgltf::iterateAccessor<hlslpp::float3>(asset.get(), accessor);
					for (auto it = iterator.begin(); it != iterator.end(); ++it) {
						hlslpp::float3 p = *it;
						positions.emplace_back(p[0]);
						positions.emplace_back(p[1]);
						positions.emplace_back(p[2]);
						positions.emplace_back(1.0f);
					}
				}

				//Loading vertex color data
				if (has_color) {
					uint64_t accessor_index = prim.findAttribute("COLOR_0")->second;
					Accessor& accessor = asset->accessors[accessor_index];
					printf("GLB primitive has %i colors\n", (int)accessor.count);
					colors.reserve(4 * accessor.count);
					auto iterator = fastgltf::iterateAccessor<hlslpp::float3>(asset.get(), accessor);
					for (auto it = iterator.begin(); it != iterator.end(); ++it) {
						hlslpp::float3 p = *it;
						colors.emplace_back(p[0]);
						colors.emplace_back(p[1]);
						colors.emplace_back(p[2]);
						colors.emplace_back(1.0f);
					}
				}

				//Load vertex uv data
				{
					uint64_t accessor_index = prim.findAttribute("TEXCOORD_0")->second;
					Accessor& accessor = asset->accessors[accessor_index];
					uvs.reserve(2 * accessor.count);
					auto iterator = fastgltf::iterateAccessor<hlslpp::float2>(asset.get(), accessor);
					for (auto it = iterator.begin(); it != iterator.end(); ++it) {
						hlslpp::float2 p = *it;
						uvs.emplace_back(p[0]);
						uvs.emplace_back(p[1]);
					}
				}
				
				//Loading index data
				{
					uint64_t idx = prim.indicesAccessor.value();
					Accessor& accessor = asset->accessors[idx];
					PRORENDER_ASSERT(accessor.componentType == ComponentType::UnsignedShort, true);
					indices.reserve(accessor.count);
					auto iterator = fastgltf::iterateAccessor<uint16_t>(asset.get(), accessor);
					for (auto it = iterator.begin(); it != iterator.end(); ++it) {
						uint16_t p = *it;
						indices.emplace_back(p);
					}
				}

				uint32_t mat_idx = std::numeric_limits<uint32_t>::max();
				if (prim.materialIndex.has_value()) {
					mat_idx = (uint32_t)prim.materialIndex.value();
				}

				GLBPrimitive p = {
					.positions = positions,
					.colors = colors,
					.uvs = uvs,
					.indices = indices,
					.material_idx = mat_idx
				};
				primitives.push_back(p);
			}
		}
	}

	GLBData g = {
		.primitives = primitives,
		.materials = materials
	};

	return g;
}