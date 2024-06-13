struct ImguiVertex {
    float2 position : POSITION;
    float2 uv : UVS;
    uint color : COLOR;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

struct ImguiPositionBlock {
    float2 positions[8];
};

struct ImguiUvBlock {
    float2 uvs[8];
};

struct ImguiColorBlock {
    uint colors[16];
};

struct ImguiVertexOutput {
    float4 position : SV_POSITION;
    float2 uv : UVS;
    float4 color : COLOR;
};

[[vk::push_constant]]
struct PushConstants {
    uint atlas_idx;
    uint sampler_idx;
    uint64_t positions_address;
    uint64_t uvs_address;
    uint64_t colors_address;
    uint64_t uniforms_address;
    float4x4 clip_matrix;
} pc;

[[vk::binding(3, 0)]]
StructuredBuffer<ImguiPositionBlock> imgui_positions;

[[vk::binding(4, 0)]]
StructuredBuffer<ImguiUvBlock> imgui_uvs;

[[vk::binding(5, 0)]]
StructuredBuffer<ImguiColorBlock> imgui_colors;