#include "ps1.hlsl"
#include "sampled_image_bindings.hlsl"
#include "instance_data.hlsl"

static const float4 HARDCODED_LIGHT = normalize(float4(1.0, 1.0, 1.0, 0.0));

float4 main(Ps1VertexOutput in_vtx) : SV_Target0 {
    
    uint64_t material_baseaddr = vk::RawBufferLoad<uint64_t>(pc.uniforms_addr + 5 * sizeof(uint64_t));
    uint64_t instance_data_baseaddr = vk::RawBufferLoad<uint64_t>(pc.uniforms_addr + 6 * sizeof(uint64_t));
    uint mat_idx_offset = sizeof(float4x4) + sizeof(uint);
    uint material_idx = vk::RawBufferLoad<uint>(instance_data_baseaddr + sizeof(GPUInstanceData) * in_vtx.instance_idx + mat_idx_offset);

    uint tex_idx = vk::RawBufferLoad<uint>(material_baseaddr + sizeof(GPUMaterial) * material_idx);
    float4 base_color = vk::RawBufferLoad<float4>(material_baseaddr + sizeof(GPUMaterial) * material_idx + sizeof(uint) * MAX_MATERIAL_TEXTURES);
    uint sampler_idx = vk::RawBufferLoad<uint>(material_baseaddr + sizeof(GPUMaterial) * material_idx + sizeof(float4) + sizeof(uint) * MAX_MATERIAL_TEXTURES);
    
    float4 color_sample = float4(1.0, 1.0, 1.0, 1.0);
    if (tex_idx != 0xFFFFFFFF) {
        color_sample = sampled_images[tex_idx].SampleLevel(samplers[sampler_idx], in_vtx.uv, 0);
    }
    //float light_attenuation = max(0.01, dot(normalize(in_vtx.world_position), HARDCODED_LIGHT));
    float light_attenuation = 1.0;

    float4 v_col = in_vtx.color;
    return base_color * v_col * float4(light_attenuation * color_sample.rgb, 1.0);
    //return float4(1.0, 0.0, 0.0, 1.0);
}