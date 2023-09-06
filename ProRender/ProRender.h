// ProRender.h : Include file for standard system include files,
// or project specific include files.

#pragma once

// TODO: Reference additional headers your program requires here.

#define VOLK_IMPLEMENTATION
#include "volk.h"

struct VulkanGraphicsDevice {
	uint32_t graphics_queue;
	uint32_t compute_queue;
	uint32_t transfer_queue;
	VkInstance instance;
	VkDevice device;
	const VkAllocationCallbacks* alloc_callbacks;
	VkCommandPool command_pool;
	VkCommandBuffer command_buffers[2];
};