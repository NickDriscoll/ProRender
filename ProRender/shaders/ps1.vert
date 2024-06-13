#include "vertex_bindings.hlsl"
#include "camera_bindings.hlsl"
#include "instance_data.hlsl"
#include "ps1.hlsl"

Ps1VertexOutput main(uint vtx_id : SV_VertexID, uint inst_idx : SV_INSTANCEID) {

    GPUInstanceData inst_data = instance_data[inst_idx];
    GPUMesh mesh = meshes[inst_data.mesh_idx];
    uint position_offset = mesh.position_start;
    uint uv_offset = mesh.uv_start;

    float4 pos = vertex_positions[(vtx_id + position_offset) / POSITION_BLOCK_SIZE].positions[(vtx_id + position_offset) % POSITION_BLOCK_SIZE];
    float2 uv = vertex_uvs[(vtx_id + uv_offset) / UV_BLOCK_SIZE].uvs[(vtx_id + uv_offset) % UV_BLOCK_SIZE];

    Camera cam = cameras[pc.camera_idx];

    float4 world_pos = mul(pos, inst_data.world_matrix);

    Ps1VertexOutput output;
    output.position = mul(mul(world_pos, cam.view_matrix), cam.projection_matrix);
    output.uv = uv;
    output.instance_idx = inst_idx;

    return output;
}
