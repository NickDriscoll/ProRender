#include "structs.hlsl"

static const float2 POSITIONS[6] = {
    -0.75, -0.75,
    -0.75, 0.75,
    0.75, -0.75,
    -0.75, 0.75,
    0.75, 0.75,
    0.75, -0.75,
};

static const float2 UVS[6] = {
    0.0, 1.0,
    0.0, 0.0,
    1.0, 1.0,
    0.0, 0.0,
    1.0, 0.0,
    1.0, 1.0
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
    float3 color = float3(UVS[idx], 0.0);
    output.color = color;
    output.position = float4(POSITIONS[idx] * (sin(pc.time) * 0.1 + 1.0), 0.0, 1.0);

    return output;
}
