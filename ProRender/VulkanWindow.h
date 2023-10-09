#pragma once

#include <vector>
#include "volk.h"
#include "VulkanGraphicsDevice.h"

struct VulkanWindow {
	//Vulkan device references
	VkInstance instance;
	VkDevice device;
	const VkAllocationCallbacks* alloc_callbacks;

	VkFormat preferred_swapchain_format;	//This seems to be a pretty standard/common swapchain format
	VkPresentModeKHR preferred_present_mode;
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkSemaphore acquire_semaphore;
	VkSemaphore present_semaphore;
	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;

	VulkanWindow(VulkanGraphicsDevice& vgd, VkSurfaceKHR surface);
	~VulkanWindow();
};