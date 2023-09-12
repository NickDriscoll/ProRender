cbuffer PushConstants
{
    uint material_idx;
    uint position_offset;
    uint tangent_offset;
    uint normal_offset;
    uint uv_offset;
}

float4 main(float4 input : POSITION) : SV_Position {
    return input;
}
