#include "vertex_bindings.hlsl"
#include "camera_bindings.hlsl"
#include "instance_data.hlsl"
#include "ps1.hlsl"
#include "util.hlsl"

Ps1VertexOutput main(uint vtx_id : SV_VertexID, uint inst_idx : SV_INSTANCEID) {

    GPUInstanceData inst_data = instance_data[inst_idx];
    GPUMesh mesh = meshes[inst_data.mesh_idx];
    uint position_offset = mesh.position_start;
    uint uv_offset = mesh.uv_start;
    
    uint64_t pos_baseaddr = vk::RawBufferLoad<uint64_t>(pc.uniforms_addr);
    float4 pos = raw_buffer_load<float4>(pos_baseaddr, sizeof(VertexPositionBlock), vtx_id + position_offset);

    uint64_t uv_baseaddr = vk::RawBufferLoad<uint64_t>(pc.uniforms_addr + sizeof(uint64_t));
    float2 uv = raw_buffer_load<float2>(uv_baseaddr, sizeof(VertexUvBlock), vtx_id + uv_offset);

    Camera cam = cameras[pc.camera_idx];

    float4 world_pos = mul(pos, inst_data.world_matrix);

    Ps1VertexOutput output;
    output.position = mul(mul(world_pos, cam.view_matrix), cam.projection_matrix);
    output.uv = uv;
    output.instance_idx = inst_idx;

    return output;
}
