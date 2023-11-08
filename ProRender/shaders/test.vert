#include "structs.hlsl"
#include "test_pushconstants.hlsl"

// static const float2 POSITIONS[6] = {
//     -0.75, -0.75,                       //Top left
//     -0.75, 0.75,                        //Bottom left
//     0.75, -0.75,                        //Top right
//     -0.75, 0.75,                        //Bottom left
//     0.75, 0.75,                         //Bottom right
//     0.75, -0.75,                        //Top right
// };

static const float2 POSITIONS[6] = {
    -1.0, -1.0,                       //Top left
    -1.0, 1.0,                        //Bottom left
    1.0, -1.0,                        //Top right
    -1.0, 1.0,                        //Bottom left
    1.0, 1.0,                         //Bottom right
    1.0, -1.0,                        //Top right
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
    float scale = 0.5;
    position *= scale;
    position += -1.0 + scale + (2.0 * scale * float2(pc.x_coord, pc.y_coord));

    float3 color = COLORS[idx];
    output.color = color;
    output.uvs = UVS[idx];
    output.position = float4(position, 0.0, 1.0);

    return output;
}
