[[vk::push_constant]]
struct PushConstants {
    float time;
} pc;

struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
};