#include "structs.hlsl"
#include "imgui.hlsl"

[[vk::binding(2, 0)]]
ConstantBuffer<FrameUniforms> frame_uniforms;

[[vk::binding(3, 0)]]
StructuredBuffer<ImguiVertex> imgui_vertices;

ImguiVertexOutput main(uint idx : SV_VertexID) {
    ImguiVertex vi = imgui_vertices[idx];

    float4x4 mat = float4x4(
        2.0/720.0, 0.0, 0.0, -1.5,
        0.0, 2.0/720.0, 0.0, -1.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );

    ImguiVertexOutput vo;
    //vo.position = mul(frame_uniforms.clip_from_screen, float4(vi.position, 0.0, 1.0));
    vo.position = mul(mat, float4(vi.position, 0.0, 1.0));
    //vo.position = float4(vi.position, 0.0, 1.0);
    vo.uv = vi.uv;
    //vo.uv = float2(0.6, 0.9);
    //vo.color = vi.color;
    vo.color.a = ((vi.color >> 24) & 0xFF) / 255.0;
    vo.color.b = ((vi.color >> 16) & 0xFF) / 255.0;
    vo.color.g = ((vi.color >> 8) & 0xFF) / 255.0;
    vo.color.r = (vi.color & 0xFF) / 255.0;

    return vo;
}
