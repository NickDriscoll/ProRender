#define POSITION_BLOCK_SIZE 4
#define UV_BLOCK_SIZE 8
#include "frame_uniform_bindings.hlsl"

struct Ps1VertexOutput {
    float4 position : SV_POSITION;
    float2 uv : UVS;
};

struct VertexPositionBlock {
    float4 positions[POSITION_BLOCK_SIZE];
};

struct VertexUvBlock {
    float2 uvs[UV_BLOCK_SIZE];
};

[[vk::binding(6, 0)]]
StructuredBuffer<VertexPositionBlock> vertex_positions;

[[vk::binding(7, 0)]]
StructuredBuffer<VertexUvBlock> vertex_uvs;

Ps1VertexOutput main(uint vtx_id : SV_VertexID) {
    float4 pos = vertex_positions[vtx_id / POSITION_BLOCK_SIZE].positions[vtx_id % POSITION_BLOCK_SIZE];
    float2 uv = vertex_uvs[vtx_id / UV_BLOCK_SIZE].uvs[vtx_id % UV_BLOCK_SIZE];

    Ps1VertexOutput output;
    output.position = pos;
    output.uv = uv;

    return output;
}