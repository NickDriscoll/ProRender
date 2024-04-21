#pragma once

#include <vector>
#include "volk.h"
#include "VulkanGraphicsDevice.h"

struct SwapchainFramebuffer {
	VulkanFrameBuffer fb;
	uint32_t idx;
};

struct VulkanWindow {
	uint32_t x_resolution;
	uint32_t y_resolution;

	//Vulkan device references
	VkInstance instance;
	VkDevice device;
	const VkAllocationCallbacks* alloc_callbacks;

	VkSurfaceFormatKHR format;
	VkPresentModeKHR preferred_present_mode;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkSemaphore acquire_semaphores[FRAMES_IN_FLIGHT];
	VkSemaphore present_semaphores[FRAMES_IN_FLIGHT];
	Key<VkRenderPass> swapchain_renderpass;
	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;
	std::vector<Key<VkFramebuffer>> swapchain_framebuffers;

	VulkanWindow(VulkanGraphicsDevice& vgd, VkSurfaceKHR surface);
	~VulkanWindow();

	SwapchainFramebuffer acquire_next_image(VulkanGraphicsDevice& vgd, SyncData& sync_data, uint64_t current_frame);
	void present_framebuffer(VulkanGraphicsDevice& vgd, SwapchainFramebuffer& swp_framebuffer, SyncData& sync_data);
	void resize(VulkanGraphicsDevice& vgd);
};