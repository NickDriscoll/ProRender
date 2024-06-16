#define POSITION_BLOCK_SIZE 4
#define UV_BLOCK_SIZE 8

struct VertexPositionBlock {
    float4 positions[POSITION_BLOCK_SIZE];
};

struct VertexUvBlock {
    float2 uvs[UV_BLOCK_SIZE];
};

struct GPUMesh {
    uint position_start;
    uint uv_start;
};
