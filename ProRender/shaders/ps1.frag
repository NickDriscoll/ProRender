#include "ps1.hlsl"
#include "sampled_image_bindings.hlsl"
#include "instance_data.hlsl"

[[vk::push_constant]]
struct {
    uint camera_idx;
} pc;

float4 main(Ps1VertexOutput in_vtx, uint inst_idx : SV_INSTANCEID) : SV_Target0 {
    GPUInstanceData inst_data = instance_data[inst_idx];
    GPUMaterial material = materials[inst_data.material_idx];

    float4 color_sample = sampled_images[material.tex_indices[0]].Sample(samplers[material.sampler_idx], in_vtx.uv);

    return float4(color_sample.rgb, 1.0);
}