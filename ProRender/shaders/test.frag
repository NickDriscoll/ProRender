#include "structs.hlsl"

[[vk::binding(0, 0)]]
Texture2D sampled_images[];

[[vk::binding(1, 0)]]
SamplerState samplers[];

float4 main(VertexOutput input) : SV_Target0 {
    float2 uvs = input.uvs;

    if (pc.image_idx == 1) {
        float uv_scale = 1.0;
        
        uvs -= float2(0.5, 0.5);
        float dist = distance(uvs, 0.0.xx);
        uvs = uvs * uv_scale;
        //uvs += pc.time * float2(-1.0, 1.0);
        float t = 0.111 * dist * 5.0 * sin(pc.time);
        float2x2 tform = float2x2(
            cos(t), -sin(t),
            sin(t), cos(t)
        );
        uvs = mul(tform, uvs);
        uvs += float2(0.5, 0.5);

        uvs.y += 0.1 * sin(pc.time + uvs.x * 4.0);

    } else if (pc.image_idx == 2) {
        uvs -= float2(0.5, 0.5);
        float dist = 2.0 * distance(uvs, 0.0.xx);
        float t = 5.0 * sin(0.2 * pc.time) * dist;
        float2x2 tform = float2x2(
            cos(t), -sin(t),
            sin(t), cos(t)
        );
        uvs = mul(tform, uvs);
        uvs += float2(0.5, 0.5);
    } else if (pc.image_idx == 3) {
        float t = 0.333 * pc.time;
        float2x2 tform = float2x2(
            cos(t), -sin(t),
            sin(t), cos(t)
        );
        uvs -= float2(0.5, 0.5);
        uvs = mul(tform, uvs);
        uvs += float2(0.5, 0.5);
        uvs.y += 0.1 * sin(pc.time + uvs.x * 4.0);
    } else {
        float2x2 tform = float2x2(
            1.0, 0.1 * sin(pc.time),
            0.0, 1.0
        );
        uvs.y -= 1.0;
        uvs = mul(tform, uvs);
        uvs.y += 1.0;
    }

    float4 image_color = sampled_images[pc.image_idx].Sample(samplers[0], uvs);
    return float4((0.125 + float3(uvs.xy, 0.0)) * image_color.rgb, image_color.a);
    //return float4(uvs, 0.0, 1.0);
}