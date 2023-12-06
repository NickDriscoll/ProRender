struct FrameUniforms {
    float4x4 clip_from_screen;
};

[[vk::binding(2, 0)]]
ConstantBuffer<FrameUniforms> frame_uniforms;