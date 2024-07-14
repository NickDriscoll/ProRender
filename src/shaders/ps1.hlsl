
[[vk::push_constant]]
struct {
    uint64_t uniforms_addr;
    uint camera_idx;
} pc;

struct Ps1VertexOutput {
    float4 position : SV_POSITION;
    float4 world_position : POSITION;
    float2 uv : UVS;
    uint instance_idx : INSTANCE;
};