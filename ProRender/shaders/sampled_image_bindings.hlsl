[[vk::binding(0, 0)]]
Texture2D sampled_images[];

[[vk::binding(1, 0)]]
SamplerState samplers[];

struct GPUMaterial {
    uint color_idx;
    float4 base_color;
};