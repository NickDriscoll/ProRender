#include "util.hlsl"
#include "imgui.hlsl"

ImguiVertexOutput main(uint idx : SV_VertexID) {
    float2 pos = raw_block_load<float2>(pc.positions_address, sizeof(ImguiPositionBlock), idx);
    float2 uv = raw_block_load<float2>(pc.uvs_address, sizeof(ImguiUvBlock), idx);
    uint packed_color = raw_block_load<uint>(pc.colors_address, sizeof(ImguiColorBlock), idx);
    
    //Color is stored as a packed uint so we have to do bit-twiddling to get the float4 color value we actually want
    float4 color = float4(
        (float)(packed_color & 0xFF) / 255.0,
        (float)((packed_color >> 8) & 0xFF) / 255.0,
        (float)((packed_color >> 16) & 0xFF) / 255.0,
        (float)((packed_color >> 24) & 0xFF) / 255.0
    );

    float4x4 clip_matrix = pc.clip_matrix;

    ImguiVertexOutput vo;
    vo.position = mul(float4(pos, 0.0, 1.0), clip_matrix);
    vo.uv = uv;
    vo.color = color;

    return vo;
}
