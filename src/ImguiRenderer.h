#include "VulkanGraphicsDevice.h"
#include "hlsl++.h"
#include <imgui.h>

#define U64_MAX 0xFFFFFFFFFFFFFFFF

struct ImguiFrame {
	uint32_t vertex_start;
	uint32_t vertex_size;
	uint32_t index_start;
	uint32_t index_size;
};

struct ImguiPushConstants {
	uint32_t atlas_idx;
	uint32_t sampler_idx;
	uint64_t position_address;
	uint64_t uv_address;
	uint64_t color_address;
    uint64_t uniforms_address;
    hlslpp::float4x4 clip_matrix;
};

struct ImguiRenderer {
    
	void draw(VkCommandBuffer& frame_cb, uint64_t frame_counter);
	uint32_t get_atlas_idx();

    ImguiRenderer(
		VulkanGraphicsDevice* v,
		ImVec2 window_size,
		Key<VkRenderPass> renderpass
	);
    ~ImguiRenderer();

private:
	uint32_t atlas_idx;		//Texture atlas index in the bindless textures array
	uint32_t sampler_idx;	//Point sampler index in the immutable samplers array

	Key<VulkanBuffer> position_buffer;
	Key<VulkanBuffer> uv_buffer;
	Key<VulkanBuffer> color_buffer;
	Key<VulkanBuffer> index_buffer;
	VkDeviceAddress position_address;
	VkDeviceAddress uv_address;
	VkDeviceAddress color_address;

	Key<VulkanGraphicsPipeline> graphics_pipeline;

	ImguiFrame frames[FRAMES_IN_FLIGHT] = {};

    VulkanGraphicsDevice* vgd;
};