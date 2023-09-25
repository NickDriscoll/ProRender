#include "structs.hlsl"

[[vk::push_constant]]
struct PushConstants {
    float time;
    uint image_idx;
} pc;

[[vk_binding(0, 0)]]
Texture2D sampled_images[];

[[vk_binding(1, 0)]]
SamplerState samplers[];

float4 main(VertexOutput input) : SV_Target0 {
    float4 image_color = sampled_images[pc.image_idx].Sample(samplers[0], input.uvs);
    return float4(image_color.rgb, 1.0);
}