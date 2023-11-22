struct ImguiVertex {
    float2 position : POSITION;
    float2 uv : UVS;
    uint color : COLOR;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

struct ImguiPositionBlock {
    float2 positions[8];
};

struct ImguiUvBlock {
    float2 uvs[8];
};

struct ImguiColorBlock {
    uint colors[16];
};

struct ImguiVertexOutput {
    float4 position : SV_POSITION;
    float2 uv : UVS;
    float4 color : COLOR;
};