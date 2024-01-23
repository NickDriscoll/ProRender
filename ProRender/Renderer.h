#include <span>
#include <stdint.h>
#include <string.h>
#include <hlsl++.h>
#include <imgui.h>
#include "slotmap.h"
#include "VulkanGraphicsDevice.h"

#define MAX_CAMERAS 64
#define MAX_VERTEX_ATTRIBS 1024

struct FrameUniforms {
	hlslpp::float4x4 clip_from_screen;
};

struct Camera {
	hlslpp::float3 position;
	float yaw;
	float pitch;

	hlslpp::float4x4 make_view_matrix();
};

struct GPUCamera {
	hlslpp::float4x4 view_matrix;
	hlslpp::float4x4 projection_matrix;
};

struct BufferView {
	uint32_t start;
	uint32_t length;
};

struct ModelAttribute {
	uint64_t position_key;
	BufferView view;
};

struct Renderer {
	uint64_t ps1_pipeline;

	VkSemaphore graphics_timeline_semaphore;

	uint64_t descriptor_set_layout_id;
	uint64_t pipeline_layout_id;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet descriptor_set;

	//Buffer of per-frame uniform data
	FrameUniforms frame_uniforms;
	uint64_t frame_uniforms_buffer;

	//Buffer of camera data
	uint64_t camera_buffer;
	slotmap<Camera> cameras;

	//Vertex buffers
	uint32_t vertex_position_offset = 0;
	uint64_t vertex_position_buffer;
	uint32_t vertex_uv_offset = 0;
	uint64_t vertex_uv_buffer;

	uint64_t push_vertex_positions(std::span<float> data);
	BufferView* get_vertex_positions(uint64_t key);
	uint64_t push_vertex_uvs(uint64_t position_key, std::span<float> data);
	BufferView* get_vertex_uvs(uint64_t key);

	uint32_t index_buffer_offset = 0;
	uint64_t index_buffer;

	uint64_t push_indices16(uint64_t position_key, std::span<uint16_t> data);
	BufferView* get_indices16(uint64_t position_key);
	
	uint64_t standard_sampler_idx;
	uint64_t point_sampler_idx;

	void record_ps1_draw();

	Renderer(VulkanGraphicsDevice* vgd, uint64_t swapchain_renderpass);
	~Renderer();

private:
	slotmap<BufferView> _position_buffers;
	slotmap<ModelAttribute> _uv_buffers;
	slotmap<ModelAttribute> _index16_buffers;
	std::vector<VkSampler> _samplers;

	VulkanGraphicsDevice* vgd;		//Very dangerous and dubiously recommended
};

ImGuiKey SDL2ToImGuiKey(int keycode);
int SDL2ToImGuiMouseButton(int button);
