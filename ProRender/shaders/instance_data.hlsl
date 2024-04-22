
struct GPUInstanceData {
	float4x4 world_matrix;
	uint mesh_idx;
	uint material_idx;
	uint _pad0;
	uint _pad1;
};

[[vk::binding(11, 0)]]
StructuredBuffer<GPUInstanceData> instance_data;