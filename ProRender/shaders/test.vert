#include "structs.hlsl"

static const float2 POSITIONS[3] = {
    0.0, -0.75,
    -0.75, 0.75,
    0.75, 0.75
};

static const float3 COLORS[3] = {
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0
};

VertexOutput main(uint idx : SV_VertexID) {
    VertexOutput output;
    output.color = COLORS[idx];
    output.position = float4(POSITIONS[idx], 0.0, 1.0);

    return output;
}
