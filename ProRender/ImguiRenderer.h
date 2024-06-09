#include "VulkanGraphicsDevice.h"
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
		uint32_t sampler,
		ImVec2 window_size,
		Key<VkPipelineLayout> pipeline_layout_id,
		Key<VkRenderPass> renderpass,
		VkDescriptorSet& descriptor_set
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

	Key<VkPipelineLayout> graphics_pipeline_layout;
	Key<VulkanGraphicsPipeline> graphics_pipeline;

	ImguiFrame frames[FRAMES_IN_FLIGHT] = {};

    VulkanGraphicsDevice* vgd;
};