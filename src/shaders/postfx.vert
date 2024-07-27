#include "postfx.hlsl"

static const float3 POSITIONS[] = {
    -1.0, -1.0, 0.0,
    -1.0, 3.0, 0.0,
    3.0, -1.0, 0.0
};

static const float2 UVS[] = {
    0.0, 0.0,
    0.0, 2.0,
    2.0, 0.0
};

PostFXVertexOutput main(uint vtx_id : SV_VertexID) {
    float3 position = POSITIONS[vtx_id];

    PostFXVertexOutput output;
    output.position = float4(position, 1.0);
    output.uv = UVS[vtx_id];
    return output;
}
