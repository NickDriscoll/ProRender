static const float PI = acos(-1);   //Most accurate Pi possible

struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
    float2 uvs : UVS;
};
