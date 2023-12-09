#include "ps1.hlsl"
#include "sampled_image_bindings.hlsl"

float4 main(Ps1VertexOutput in_vtx) : SV_Target0 {

    return float4(in_vtx.uv.x, in_vtx.uv.y, 0.0, 1.0);
}