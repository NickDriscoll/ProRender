#include "structs.hlsl"

[[vk::push_constant]]
struct PushConstants {
    float time;
    uint image_idx;
} pc;

static const float2 POSITIONS[6] = {
    -0.75, -0.75,                       //Top left
    -0.75, 0.75,                        //Bottom left
    0.75, -0.75,                        //Top right
    -0.75, 0.75,                        //Bottom left
    0.75, 0.75,                         //Bottom right
    0.75, -0.75,                        //Top right
};

static const float2 UVS[6] = {
    0.0, 0.0,
    0.0, 1.0,
    1.0, 0.0,
    0.0, 1.0,
    1.0, 1.0,
    1.0, 0.0
};

static const float3 COLORS[6] = {
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0,
    0.0, 1.0, 0.0,
    1.0, 0.0, 0.0,
    0.0, 0.0, 1.0,
};

VertexOutput main(uint idx : SV_VertexID) {
    VertexOutput output;
    //float2 position = POSITIONS[idx] * (sin(pc.time) * 0.1 + 1.0);
    float2 position = POSITIONS[idx];
    float3 color = float3(UVS[idx], 0.0);
    output.color = color;
    output.uvs = UVS[idx];
    output.position = float4(position, 0.0, 1.0);

    return output;
}
