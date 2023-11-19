#include "structs.hlsl"
#include "imgui.hlsl"

[[vk::binding(2, 0)]]
ConstantBuffer<FrameUniforms> frame_uniforms;

[[vk::binding(3, 0)]]
StructuredBuffer<ImguiPositionBlock> imgui_positions;

[[vk::binding(4, 0)]]
StructuredBuffer<ImguiUvBlock> imgui_uvs;

[[vk::binding(5, 0)]]
StructuredBuffer<ImguiColorBlock> imgui_colors;

ImguiVertexOutput main(uint idx : SV_VertexID) {
    float2 pos = imgui_positions[idx / 8].positions[idx % 8];

    float2 uv = imgui_uvs[idx / 8].uvs[idx % 8];
    uint color = imgui_colors[idx / 16].colors[idx % 16];

    ImguiVertexOutput vo;
    vo.position = mul(float4(pos, 0.0, 1.0), frame_uniforms.clip_from_screen);
    vo.uv = uv;
    vo.color = color;

    return vo;
}
