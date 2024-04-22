#define POSITION_BLOCK_SIZE 4
#define UV_BLOCK_SIZE 8

struct VertexPositionBlock {
    float4 positions[POSITION_BLOCK_SIZE];
};

[[vk::binding(6, 0)]]
StructuredBuffer<VertexPositionBlock> vertex_positions;

struct VertexUvBlock {
    float2 uvs[UV_BLOCK_SIZE];
};

[[vk::binding(7, 0)]]
StructuredBuffer<VertexUvBlock> vertex_uvs;

struct GPUMesh {
    uint position_start;
    uint uv_start;
};

[[vk::binding(9, 0)]]
StructuredBuffer<GPUMesh> meshes;
