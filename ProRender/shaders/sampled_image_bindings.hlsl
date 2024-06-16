[[vk::binding(0, 0)]]
Texture2D sampled_images[];

[[vk::binding(1, 0)]]
SamplerState samplers[];

#define MAX_MATERIAL_TEXTURES 8
struct GPUMaterial {
    uint tex_indices[MAX_MATERIAL_TEXTURES];
    float4 base_color;
    uint sampler_idx;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};