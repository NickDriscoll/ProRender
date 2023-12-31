#include "vertex_bindings.hlsl"
#include "camera_bindings.hlsl"
#include "ps1.hlsl"

[[vk::push_constant]]
struct {
    uint position_offset;
    uint uv_offset;
    uint camera_idx;
    uint texture_id;
} pc;

Ps1VertexOutput main(uint vtx_id : SV_VertexID) {
    float4 pos = vertex_positions[(vtx_id + pc.position_offset) / POSITION_BLOCK_SIZE].positions[(vtx_id + pc.position_offset) % POSITION_BLOCK_SIZE];
    float2 uv = vertex_uvs[(vtx_id + pc.uv_offset) / UV_BLOCK_SIZE].uvs[(vtx_id + pc.uv_offset) % UV_BLOCK_SIZE];

    Camera cam = cameras[pc.camera_idx];

    Ps1VertexOutput output;
    output.position = mul(mul(pos, cam.view_matrix), cam.projection_matrix);
    output.uv = uv;

    return output;
}