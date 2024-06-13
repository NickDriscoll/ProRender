#include "frame_uniform_bindings.hlsl"
#include "structs.hlsl"
#include "imgui.hlsl"

template<typename T>
T raw_buffer_load(uint64_t base_addr, uint blocksize, uint idx) {
    uint items_per_block = 64 / sizeof(T);
    uint64_t addr = base_addr + blocksize * (idx / items_per_block);
    addr += sizeof(T) * (idx % items_per_block);
    return vk::RawBufferLoad<T>(addr);
}

ImguiVertexOutput main(uint idx : SV_VertexID) {
    float2 pos = raw_buffer_load<float2>(pc.positions_address, sizeof(ImguiPositionBlock), idx);
    float2 uv = raw_buffer_load<float2>(pc.uvs_address, sizeof(ImguiUvBlock), idx);
    uint packed_color = raw_buffer_load<uint>(pc.colors_address, sizeof(ImguiColorBlock), idx);
    
    //Color is stored as a packed uint so we have to do bit-twiddling to get the float4 color value we actually want
    float4 color = float4(
        (float)(packed_color & 0xFF) / 255.0,
        (float)((packed_color >> 8) & 0xFF) / 255.0,
        (float)((packed_color >> 16) & 0xFF) / 255.0,
        (float)((packed_color >> 24) & 0xFF) / 255.0
    );

    //float4x4 clip_matrix = frame_uniforms.clip_from_screen;
    float4x4 clip_matrix = pc.clip_matrix;

    ImguiVertexOutput vo;
    vo.position = mul(float4(pos, 0.0, 1.0), clip_matrix);
    vo.uv = uv;
    vo.color = color;

    return vo;
}
