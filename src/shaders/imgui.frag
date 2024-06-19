#include "imgui.hlsl"
#include "util.hlsl"
#include "sampled_image_bindings.hlsl"

float4 main(ImguiVertexOutput input) : SV_Target0 {
    float4 atlas_sample = sampled_images[pc.atlas_idx].Sample(samplers[pc.sampler_idx], input.uv);
    return atlas_sample * input.color;
}