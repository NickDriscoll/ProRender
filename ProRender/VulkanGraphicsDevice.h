#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include "volk.h"
#define FRAMES_IN_FLIGHT 2		//Number of simultaneous frames the GPU could be working on

struct VulkanGraphicsDevice {
	uint32_t graphics_queue_family_idx;
	uint32_t compute_queue_family_idx;
	uint32_t transfer_queue_family_idx;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	const VkAllocationCallbacks* alloc_callbacks;
	VkCommandPool command_pool;
	VkCommandBuffer command_buffers[FRAMES_IN_FLIGHT];
	VkPipelineCache pipeline_cache;

	//These two fields can be members of the graphics device struct because
	//we are assuming bindless resource management
	VkDescriptorSetLayout descriptor_set_layout;
	VkPipelineLayout pipeline_layout;

    void init();

	//void thing();
};