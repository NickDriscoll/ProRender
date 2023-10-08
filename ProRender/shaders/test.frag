#include "structs.hlsl"

[[vk::binding(0, 0)]]
Texture2D sampled_images[];

[[vk::binding(1, 0)]]
SamplerState samplers[];

float4 main(VertexOutput input) : SV_Target0 {
    float2 uvs = input.uvs;
    uvs.y += 0.1 * sin(pc.time + uvs.x * 4.0);

    if (pc.image_idx == 1) {
        float uv_scale = 2.0 * (sin(pc.time) * 0.5 + 0.5);
        uvs = uvs * uv_scale - uv_scale / 2.0;
    } else if (pc.image_idx == 2) {
        float t = 0.333 * pc.time;
        float2x2 tform = float2x2(
            cos(t), -sin(t),
            sin(t), cos(t)
        );
        uvs -= float2(0.5, 0.5);
        uvs = mul(tform, uvs);
        uvs += float2(0.5, 0.5);
    }

    float4 image_color = sampled_images[pc.image_idx].Sample(samplers[0], uvs);
    return float4((0.5 + input.color) * image_color.rgb, 1.0);
    //return float4(uvs, 0.0, 1.0);
}