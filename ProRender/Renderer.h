#include <span>
#include <stdint.h>
#include <string.h>
#include <hlsl++.h>
#include <imgui.h>
#include "slotmap.h"
#include "VulkanGraphicsDevice.h"

#define MAX_CAMERAS 64
#define MAX_MATERIALS 1024
#define MAX_VERTEX_ATTRIBS 1024

enum DescriptorBindings : uint32_t {
	SAMPLED_IMAGES = 0,
	SAMPLERS = 1,
	FRAME_UNIFORMS = 2,
	IMGUI_POSITIONS = 3,
	IMGUI_UVS = 4,
	IMGUI_COLORS = 5,
	VERTEX_POSITIONS = 6,
	VERTEX_UVS = 7,
	CAMERA_BUFFER = 8
};

struct FrameUniforms {
	hlslpp::float4x4 clip_from_screen;
};

struct Camera {
	hlslpp::float3 position;
	float yaw;
	float pitch;
	float roll;

	hlslpp::float4x4 make_view_matrix();
};

struct GPUCamera {
	hlslpp::float4x4 view_matrix;
	hlslpp::float4x4 projection_matrix;
};

struct GPUMesh{
	uint32_t position_start;
	uint32_t uv_start;
};

struct GPUMaterial {
	uint32_t color_idx;
	uint32_t sampler_idx;
	hlslpp::float4 base_color;
};

struct GPUInstanceData {
	hlslpp::float4x4 world_matrix;
	uint32_t mesh_idx;
	uint32_t material_idx;
};

struct BufferView {
	uint32_t start;
	uint32_t length;
};

struct MeshAttribute {
	Key<BufferView> position_key;
	BufferView view;
};

struct Material {
	uint64_t batch_id;		//The batch where this material's textures come from
	uint32_t sampler_idx;
	hlslpp::float4 base_color;
};

struct VulkanRenderer {
	Key<VulkanGraphicsPipeline> ps1_pipeline;

	VkSemaphore graphics_timeline_semaphore;

	Key<VkDescriptorSetLayout> descriptor_set_layout_id;
	Key<VkPipelineLayout> pipeline_layout_id;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet descriptor_set;

	//Buffer of per-frame uniform data
	FrameUniforms frame_uniforms;
	Key<VulkanBuffer> frame_uniforms_buffer;

	//Buffer of camera data
	Key<VulkanBuffer> camera_buffer;
	slotmap<Camera> cameras;

	//Vertex buffers
	uint32_t vertex_position_offset = 0;
	Key<VulkanBuffer> vertex_position_buffer;
	uint32_t vertex_uv_offset = 0;
	Key<VulkanBuffer> vertex_uv_buffer;

	Key<BufferView> push_vertex_positions(std::span<float> data);
	BufferView* get_vertex_positions(Key<BufferView> key);
	Key<MeshAttribute> push_vertex_uvs(Key<BufferView> position_key, std::span<float> data);
	BufferView* get_vertex_uvs(Key<BufferView> key);

	//Buffer for storing all loaded mesh indices
	uint32_t index_buffer_offset = 0;
	Key<VulkanBuffer> index_buffer;

	Key<MeshAttribute> push_indices16(Key<BufferView> position_key, std::span<uint16_t> data);
	BufferView* get_indices16(Key<BufferView> position_key);

	//Idea is that the first image in a batch is treated as the color image
	//and other texture types will be assigned indices later
	Key<Material> push_material(uint64_t batch_id, hlslpp::float4& base_color);
	
	uint32_t standard_sampler_idx;
	uint32_t point_sampler_idx;

	void ps1_draw(Key<BufferView> mesh_key, Key<Material> material_key, std::span<hlslpp::float4x4>& world_transform);

	//Called at the end of each frame to render frame's draw calls
	void render();

	VulkanRenderer(VulkanGraphicsDevice* vgd, Key<VkRenderPass> swapchain_renderpass);
	~VulkanRenderer();

private:
	slotmap<BufferView> _position_buffers;
	slotmap<MeshAttribute> _uv_buffers;
	slotmap<MeshAttribute> _index16_buffers;
	slotmap<Material> _materials;
	std::vector<VkSampler> _samplers;
	std::vector<VkDrawIndexedIndirectCommand> draw_calls;	//Reset every frame

	Key<VulkanBuffer> material_buffer;

	VulkanGraphicsDevice* vgd;		//Very dangerous and dubiously recommended
};
