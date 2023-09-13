// ProRender.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#define _CRT_SECURE_NO_WARNINGS

// TODO: Reference additional headers your program requires here.

#include <filesystem>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include "SDL.h"
#include "SDL_main.h"
#include "SDL_vulkan.h"

#ifdef WIN32
	#define VK_USE_PLATFORM_WIN32_KHR
#endif

#ifdef __linux__
	#define VK_USE_PLATFORM_XLIB_KHR
#endif

#define VOLK_IMPLEMENTATION
#include "volk.h"

#define FRAMES_IN_FLIGHT 2		//Number of simultaneous frames the GPU could be working on

struct VulkanGraphicsDevice {
	uint32_t graphics_queue;
	uint32_t compute_queue;
	uint32_t transfer_queue;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
	const VkAllocationCallbacks* alloc_callbacks;
	VkCommandPool command_pool;
	VkCommandBuffer command_buffers[FRAMES_IN_FLIGHT];
	VkPipelineCache pipeline_cache;



	//void thing();
};