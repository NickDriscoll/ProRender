
[[vk::push_constant]]
struct {
    uint camera_idx;
} pc;

struct Ps1VertexOutput {
    float4 position : SV_POSITION;
    float2 uv : UVS;
    uint instance_idx : INSTANCE;
};