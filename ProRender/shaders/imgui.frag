#include "imgui.hlsl"
#include "structs.hlsl"

[[vk::binding(0, 0)]]
Texture2D sampled_images[];

[[vk::binding(1, 0)]]
SamplerState samplers[];

[[vk::binding(2, 0)]]
ConstantBuffer<FrameUniforms> frame_uniforms;

[[vk::push_constant]]
struct PushConstants {
    uint atlas_idx;
    uint sampler_idx;
} pc;

float4 main(ImguiVertexOutput input) : SV_Target0 {

    float4 atlas_sample = sampled_images[0].Sample(samplers[0], input.uv);
    if (atlas_sample.a == 0.0)
        discard;
    
    return atlas_sample * input.color;
}