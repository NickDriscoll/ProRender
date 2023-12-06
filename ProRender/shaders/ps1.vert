#include "frame_uniform_bindings.hlsl"
#include "vertex_bindings.hlsl"
#include "ps1.hlsl"

Ps1VertexOutput main(uint vtx_id : SV_VertexID) {
    float4 pos = vertex_positions[vtx_id / POSITION_BLOCK_SIZE].positions[vtx_id % POSITION_BLOCK_SIZE];
    float2 uv = vertex_uvs[vtx_id / UV_BLOCK_SIZE].uvs[vtx_id % UV_BLOCK_SIZE];

    Ps1VertexOutput output;
    output.position = pos;
    output.uv = uv;

    return output;
}