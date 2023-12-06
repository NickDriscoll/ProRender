#define POSITION_BLOCK_SIZE 4
#define UV_BLOCK_SIZE 8

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