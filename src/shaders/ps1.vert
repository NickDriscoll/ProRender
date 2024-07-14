#include "vertex_bindings.hlsl"
#include "camera_bindings.hlsl"
#include "instance_data.hlsl"
#include "ps1.hlsl"
#include "util.hlsl"

Ps1VertexOutput main(uint vtx_id : SV_VertexID, uint inst_idx : SV_INSTANCEID) {
    //I hate HLSL
    uint64_t pos_baseaddr = vk::RawBufferLoad<uint64_t>(pc.uniforms_addr);
    uint64_t uv_baseaddr = vk::RawBufferLoad<uint64_t>(pc.uniforms_addr + sizeof(uint64_t));
    uint64_t cam_baseaddr = vk::RawBufferLoad<uint64_t>(pc.uniforms_addr + 2 * sizeof(uint64_t));
    uint64_t mesh_baseaddr = vk::RawBufferLoad<uint64_t>(pc.uniforms_addr + 3 * sizeof(uint64_t));
    uint64_t instance_data_baseaddr = vk::RawBufferLoad<uint64_t>(pc.uniforms_addr + 5 * sizeof(uint64_t));

    float4x4 world_matrix = vk::RawBufferLoad<float4x4>(instance_data_baseaddr + sizeof(GPUInstanceData) * inst_idx);
    uint mesh_idx = vk::RawBufferLoad<uint>(instance_data_baseaddr + sizeof(GPUInstanceData) * inst_idx + sizeof(float4x4));

    uint position_offset = vk::RawBufferLoad<uint>(mesh_baseaddr + sizeof(GPUMesh) * mesh_idx);
    uint uv_offset = vk::RawBufferLoad<uint>(mesh_baseaddr + sizeof(GPUMesh) * mesh_idx + sizeof(uint));
    
    uint position_float4_offset = (vtx_id + (position_offset / 4));
    uint uv_float2_offset = (vtx_id + (uv_offset / 2));
    float4 pos = raw_block_load<float4>(pos_baseaddr, sizeof(VertexPositionBlock), position_float4_offset);
    float2 uv = raw_block_load<float2>(uv_baseaddr, sizeof(VertexUvBlock), uv_float2_offset);

    //For some reason float4x4's memory ordering is different than when using descriptors
    float4x4 view_matrix = vk::RawBufferLoad<float4x4>(cam_baseaddr);
    float4x4 projection_matrix = vk::RawBufferLoad<float4x4>(cam_baseaddr + sizeof(Camera) * pc.camera_idx + sizeof(float4x4));


    Ps1VertexOutput output;
    output.world_position = mul(world_matrix, pos);
    output.position = mul(projection_matrix, mul(view_matrix, output.world_position));
    output.uv = uv;
    output.instance_idx = inst_idx;

    return output;
}
