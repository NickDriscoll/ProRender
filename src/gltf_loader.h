#include "volk.h"
#include <hlsl++.h>
#include <filesystem>
#include <vector>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>


template <>
struct fastgltf::ElementTraits<hlslpp::float2> : fastgltf::ElementTraitsBase<hlslpp::float2, AccessorType::Vec2, float> {};
template <>
struct fastgltf::ElementTraits<hlslpp::float3> : fastgltf::ElementTraitsBase<hlslpp::float3, AccessorType::Vec3, float> {};

struct GLBMaterial {
	hlslpp::float4 base_color;
	std::vector<uint8_t> color_image_bytes;
	VkFormat color_image_format;
};

struct GLBPrimitive {
	std::vector<float> positions;
	std::vector<float> colors;
	std::vector<float> uvs;
	std::vector<uint16_t> indices;
	uint32_t material_idx;
};

//Data extracted from a .glb file, ready to be ingested by a renderer
struct GLBData {
	std::vector<GLBPrimitive> primitives;
	std::vector<uint16_t> prim_parents;
	std::vector<GLBMaterial> materials;
};

GLBData load_glb(const std::filesystem::path& glb_path);