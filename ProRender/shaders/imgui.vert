#include "structs.hlsl"
#include "imgui.hlsl"

[[vk::binding(2, 0)]]
ConstantBuffer<FrameUniforms> frame_uniforms;

ImguiVertexOutput main(uint idx : SV_VertexID) {
    float2 pos = imgui_positions[idx / 8].positions[idx % 8];

    float2 uv = imgui_uvs[idx / 8].uvs[idx % 8];
    uint packed_color = imgui_colors[idx / 16].colors[idx % 16];
    
    //Color is stored as a packed uint so we have to do bit-twiddling to get the float4 color value we actually want
    float4 color = float4(
        (float)(packed_color & 0xFF) / 255.0,
        (float)((packed_color >> 8) & 0xFF) / 255.0,
        (float)((packed_color >> 16) & 0xFF) / 255.0,
        (float)((packed_color >> 24) & 0xFF) / 255.0
    );

    ImguiVertexOutput vo;
    vo.position = mul(float4(pos, 0.0, 1.0), frame_uniforms.clip_from_screen);
    vo.uv = uv;
    vo.color = color;

    return vo;
}
