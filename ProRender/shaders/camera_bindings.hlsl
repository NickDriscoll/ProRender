struct Camera {
	float4x4 view_matrix;
	float4x4 projection_matrix;
};

[[vk::binding(5, 0)]]
StructuredBuffer<Camera> cameras;