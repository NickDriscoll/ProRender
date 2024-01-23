#include "VulkanGraphicsDevice.h"
#include "VulkanWindow.h"
#include <imgui.h>

#define U64_MAX 0xFFFFFFFFFFFFFFFF

struct ImguiFrame {
	uint32_t vertex_start;
	uint32_t vertex_size;
	uint32_t index_start;
	uint32_t index_size;
};

struct ImguiRenderer {
    
	void draw(VkCommandBuffer& frame_cb, uint64_t frame_counter);
	uint32_t get_atlas_idx();

    ImguiRenderer(
		VulkanGraphicsDevice* v,
		uint64_t sampler,
		ImVec2 window_size,
		uint64_t pipeline_layout_id,
		uint64_t renderpass,
		VkDescriptorSet& descriptor_set
	);
    ~ImguiRenderer();

private:
	uint32_t atlas_idx;
	uint32_t sampler_idx;
	uint64_t position_buffer;
	uint64_t uv_buffer;
	uint64_t color_buffer;
	uint64_t index_buffer;
	uint64_t graphics_pipeline_layout;
	uint64_t graphics_pipeline;
	ImguiFrame frames[FRAMES_IN_FLIGHT] = {};

    VulkanGraphicsDevice* vgd;
};