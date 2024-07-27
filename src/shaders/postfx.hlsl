struct PostFXVertexOutput {
    float4 position : SV_POSITION;
    float2 uv : UVS;
};

[[vk::push_constant]]
struct {
    uint color_buffer_idx;
} pc;
