#include "structs.hlsl"
#include "imgui.hlsl"

[[vk::binding(2, 0)]]
ConstantBuffer<FrameUniforms> frame_uniforms;

[[vk::binding(3, 0)]]
StructuredBuffer<ImguiVertex> imgui_vertices;

ImguiVertexOutput main(uint idx : SV_VertexID) {
    ImguiVertex vi = imgui_vertices[idx];

    ImguiVertexOutput vo;
    vo.position = mul(float4(vi.position, 0.0, 1.0), frame_uniforms.clip_from_screen);
    vo.uv = vi.uv;
    vo.color = vi.color;

    return vo;
}
