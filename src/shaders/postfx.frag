#include "postfx.hlsl"
#include "sampled_image_bindings.hlsl"

float4 main(PostFXVertexOutput in_vtx) : SV_Target0 {
    
    float4 color_sample = sampled_images[pc.color_buffer_idx].Sample(samplers[1], in_vtx.uv);
    return float4(color_sample.rgb, 1.0);
}