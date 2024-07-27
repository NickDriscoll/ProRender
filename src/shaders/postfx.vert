static const float3 POSITIONS[] = {
    -1.0, -1.0, 0.0,
    3.0, -1.0, 0.0,
    -1.0, 3.0, 0.0
};

PostFXVertexOutput main(uint vtx_id : SV_VertexID) {
    float3 position = POSITIONS[vtx_id];
}
