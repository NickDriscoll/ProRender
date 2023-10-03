#include "structs.hlsl"

[[vk::push_constant]]
struct PushConstants {
    float time;
    uint image_idx;
} pc;

[[vk::binding(0, 0)]]
Texture2D sampled_images[];

[[vk::binding(1, 0)]]
SamplerState samplers[];

float4 main(VertexOutput input) : SV_Target0 {
    float2 uvs = input.uvs;
    //uvs.y += 0.1 * sin(pc.time + uvs.x * 4.0);
    float4 image_color = sampled_images[pc.image_idx].Sample(samplers[0], uvs);
    return float4(image_color.rgb, 1.0);
    //return float4(uvs, 0.0, 1.0);
}