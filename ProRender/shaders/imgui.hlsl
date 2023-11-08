struct ImguiVertex {
    float2 position : POSITION;
    float2 uv : UVS;
    float4 color : COLOR;
};

struct ImguiVertexOutput {
    float4 position : SV_POSITION;
    float2 uv : UVS;
    float4 color : COLOR;
};