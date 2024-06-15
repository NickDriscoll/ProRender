struct FrameUniforms {
	uint64_t positions_addr;
	uint64_t uvs_addr;
	uint64_t cameras_addr;
	uint64_t meshes_addr;
	uint64_t materials_addr;
	uint64_t instancedata_addr;
};

[[vk::binding(2, 0)]]
ConstantBuffer<FrameUniforms> frame_uniforms;