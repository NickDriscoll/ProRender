struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
    float2 uvs : UVS;
};

struct FrameUniforms {
    float4x4 clip_from_screen;
};