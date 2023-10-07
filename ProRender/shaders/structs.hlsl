struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
    float2 uvs : UVS;
};

[[vk::push_constant]]
struct PushConstants {
    float time;
    uint image_idx;
    uint x_coord;
    uint y_coord;
} pc;