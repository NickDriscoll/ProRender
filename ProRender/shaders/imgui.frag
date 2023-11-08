#include "imgui.hlsl"
#include "structs.hlsl"

[[vk::binding(0, 0)]]
Texture2D sampled_images[];

[[vk::binding(1, 0)]]
SamplerState samplers[];

[[vk::binding(2, 0)]]
StructuredBuffer<FrameUniforms> frame_uniforms;

float4 main(ImguiVertexOutput input) : SV_Target0 {

    float4 atlas_sample = sampled_images[0].Sample(samplers[0], input.uv);
    
    return atlas_sample;
}