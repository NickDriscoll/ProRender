#include "ps1.hlsl"
#include "sampled_image_bindings.hlsl"
#include "instance_data.hlsl"

float4 main(Ps1VertexOutput in_vtx) : SV_Target0 {
    
    uint64_t material_baseaddr = vk::RawBufferLoad<uint64_t>(pc.uniforms_addr + 4 * sizeof(uint64_t));
    uint64_t instance_data_baseaddr = vk::RawBufferLoad<uint64_t>(pc.uniforms_addr + 5 * sizeof(uint64_t));
    uint mat_idx_offset = sizeof(float4x4) + sizeof(uint);
    uint material_idx = vk::RawBufferLoad<uint>(instance_data_baseaddr + sizeof(GPUInstanceData) * in_vtx.instance_idx + mat_idx_offset);

    uint tex_idx = vk::RawBufferLoad<uint>(material_baseaddr + sizeof(GPUMaterial) * material_idx);
    uint sampler_idx = vk::RawBufferLoad<uint>(material_baseaddr + sizeof(GPUMaterial) * material_idx + sizeof(float4) + sizeof(uint) * MAX_MATERIAL_TEXTURES);
    
    float4 color_sample = sampled_images[tex_idx].Sample(samplers[sampler_idx], in_vtx.uv);

    return float4(color_sample.rgb, 1.0);
}