struct Camera {
	float4x4 view_matrix;
	float4x4 projection_matrix;
};

[[vk::binding(8, 0)]]
ConstantBuffer<Camera> cameras;