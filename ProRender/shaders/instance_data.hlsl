
struct GPUInstanceData {
	float4x4 world_matrix;
	uint mesh_idx;
	uint material_idx;
	uint _pad0;
	uint _pad1;
};