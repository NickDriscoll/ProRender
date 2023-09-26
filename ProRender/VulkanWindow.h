#pragma once

#include <vector>
#include "volk.h"
#include "VulkanGraphicsDevice.h"

struct VulkanWindow {
	VkFormat preferred_swapchain_format;	//This seems to be a pretty standard/common swapchain format
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkSemaphore acquire_semaphore;
	VkSemaphore present_semaphore;
	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;

	void init(VulkanGraphicsDevice& vgd, VkSurfaceKHR surface);
};