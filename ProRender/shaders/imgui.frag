#include "imgui.hlsl"
#include "structs.hlsl"
#include "sampled_image_bindings.hlsl"

[[vk::push_constant]]
struct PushConstants {
    uint atlas_idx;
    uint sampler_idx;
} pc;

float4 main(ImguiVertexOutput input) : SV_Target0 {
    float4 atlas_sample = sampled_images[pc.atlas_idx].Sample(samplers[pc.sampler_idx], input.uv);
    if (atlas_sample.a == 0.0)
        discard;
    
    return atlas_sample * input.color;
}