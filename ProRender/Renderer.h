#include <span>
#include <stdint.h>
#include <string.h>
#include <hlsl++.h>
#include <imgui.h>
#include "slotmap.h"
#include "VulkanGraphicsDevice.h"

#define MAX_CAMERAS 64

struct FrameUniforms {
	hlslpp::float4x4 clip_from_screen;
};

struct Camera {
	hlslpp::float3 position;
	float yaw;
	float pitch;
};

struct GPUCamera {
	hlslpp::float4x4 view_matrix;
	hlslpp::float4x4 projection_matrix;
};

struct BufferView {
	uint32_t start;
	uint32_t length;
};

struct ImguiFrame {
	uint32_t vertex_start;
	uint32_t vertex_size;
	uint32_t index_start;
	uint32_t index_size;
};

struct Renderer {
	uint64_t descriptor_set_layout_id;
	uint64_t pipeline_layout_id;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet descriptor_set;

	//Buffer of per-frame uniform data
	FrameUniforms frame_uniforms;
	uint64_t frame_uniforms_buffer;

	//Buffer of camera data
	uint64_t camera_buffer;
	uint64_t main_viewport_camera;
	slotmap<Camera> cameras;

	//Vertex buffers
	uint32_t vertex_position_offset = 0;
	uint64_t vertex_position_buffer;
	uint32_t vertex_uv_offset = 0;
	uint64_t vertex_uv_buffer;

	uint32_t index_buffer_offset = 0;
	uint64_t index_buffer;

	//TODO: Imgui data probably shouldn't be directly in init
	//uint64_t imgui_vertex_buffer;
	uint32_t imgui_atlas_idx;
	uint32_t imgui_sampler_idx;
	uint64_t imgui_position_buffer;
	uint64_t imgui_uv_buffer;
	uint64_t imgui_color_buffer;
	uint64_t imgui_index_buffer;
	ImguiFrame imgui_frames[FRAMES_IN_FLIGHT] = {};

	BufferView push_vertex_positions(std::span<float> data);
	BufferView push_vertex_uvs(std::span<float> data);
	BufferView push_indices16(std::span<uint16_t> data);

	Renderer(VulkanGraphicsDevice* vgd);
	~Renderer();

private:
	std::vector<VkSampler> _samplers;
	VulkanGraphicsDevice* vgd;		//Very dangerous and dubiously recommended
};

ImGuiKey SDL2ToImGuiKey(int keycode);
int SDL2ToImGuiMouseButton(int button);
