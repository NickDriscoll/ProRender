#include <stdint.h>
#include <hlsl++.h>
#include "VulkanGraphicsDevice.h"

struct FrameUniforms {
	hlslpp::float4x4 clip_from_screen;
};

struct ImguiFrame {
	uint32_t vertex_start;
	uint32_t vertex_size;
	uint32_t index_start;
	uint32_t index_size;
};

struct Renderer {

	//Buffer of per-frame uniform data
	FrameUniforms frame_uniforms;
	uint64_t frame_uniforms_buffer;

	//TODO: Imgui data probably shouldn't be directly in init
	//uint64_t imgui_vertex_buffer;
	uint64_t imgui_position_buffer;
	uint64_t imgui_uv_buffer;
	uint64_t imgui_color_buffer;
	uint64_t imgui_index_buffer;
	ImguiFrame imgui_frames[FRAMES_IN_FLIGHT] = {};

	Renderer(VulkanGraphicsDevice* vgd);
	~Renderer();

private:
	VulkanGraphicsDevice* vgd;		//Very dangerous and dubiously recommended
};

enum ImGuiKey SDL2ToImGuiKey(int keycode);
int SDL2ToImGuiMouseButton(int button);