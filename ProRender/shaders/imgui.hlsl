struct ImguiVertex {
    float2 position : POSITION;
    float2 uv : UVS;
    uint color : COLOR;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

struct ImguiVertexOutput {
    float4 position : SV_POSITION;
    float2 uv : UVS;
    uint color : COLOR;
};