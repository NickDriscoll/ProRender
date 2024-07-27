#include "postfx.hlsl"
#include "sampled_image_bindings.hlsl"

float4 main(PostFXVertexOutput in_vtx) : SV_Target0 {
    
    return float4(in_vtx.uv, 0.0, 1.0);
}