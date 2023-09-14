struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
};

float4 main(VertexOutput input) : SV_Target0 {
    return float4(input.color, 1.0);
}