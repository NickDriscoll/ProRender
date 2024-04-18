#pragma once

#include <vector>
#include "volk.h"
#include "VulkanGraphicsDevice.h"

struct VulkanWindow {
	uint32_t x_resolution;
	uint32_t y_resolution;

	//Vulkan device references
	VkInstance instance;
	VkDevice device;
	const VkAllocationCallbacks* alloc_callbacks;

	VkSurfaceFormatKHR format;	//This seems to be a pretty standard/common swapchain format
	VkPresentModeKHR preferred_present_mode;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkSemaphore acquire_semaphores[FRAMES_IN_FLIGHT];
	VkSemaphore present_semaphores[FRAMES_IN_FLIGHT];
	Key<VkRenderPass> swapchain_renderpass;
	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;
	std::vector<VkFramebuffer> swapchain_framebuffers;

	VulkanWindow(VulkanGraphicsDevice& vgd, VkSurfaceKHR surface);
	~VulkanWindow();

	VulkanFrameBuffer acquire_framebuffer(VulkanGraphicsDevice& vgd, uint64_t current_frame);
	void resize(VulkanGraphicsDevice& vgd);
};