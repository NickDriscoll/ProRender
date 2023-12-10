#include "ps1.hlsl"
#include "sampled_image_bindings.hlsl"

[[vk::push_constant]]
struct {
    uint position_offset;
    uint uv_offset;
    uint camera_idx;
    uint texture_id;
    uint sampler_id;
} pc;

float4 main(Ps1VertexOutput in_vtx) : SV_Target0 {

    float4 color_sample = sampled_images[pc.texture_id].Sample(samplers[pc.sampler_id], in_vtx.uv);

    return float4(color_sample.rgb, 1.0);
}