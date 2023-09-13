static const float2 TRIANGLE[3] = {
    0.5, 0.25,
    0.25, 0.75,
    0.75,0.75
};

float4 main(uint idx : SV_VertexID) : SV_Position {
    return float4(TRIANGLE[idx], 0.0, 1.0);
}
