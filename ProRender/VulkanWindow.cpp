#include "VulkanWindow.h"

void VulkanWindow::init(VulkanGraphicsDevice& vgd, VkSurfaceKHR surface) {
    //Save surface
    this->surface = surface;

    //Query for surface capabilities
	VkSurfaceCapabilitiesKHR surface_capabilities = {};
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vgd.physical_device, surface, &surface_capabilities) != VK_SUCCESS) {
		printf("Getting VkSurface capabilities failed.\n");
		exit(-1);
	}

	uint32_t format_count;
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(vgd.physical_device, surface, &format_count, nullptr) != VK_SUCCESS) {
		printf("Getting surface format count failed.\n");
		exit(-1);
	}

	std::vector<VkSurfaceFormatKHR> formats;
	formats.resize(format_count);
	if (vkGetPhysicalDeviceSurfaceFormatsKHR(vgd.physical_device, surface, &format_count, formats.data()) != VK_SUCCESS) {
		printf("Getting surface formats failed.\n");
		exit(-1);
	}

	bool found_preferred_format = false;
	for (uint32_t i = 0; i < format_count; i++) {
		VkFormat format = formats[i].format;
		if (format == preferred_swapchain_format) {
			found_preferred_format = true;
			break;
		}
	}
	if (!found_preferred_format) {
		printf("Surface doesn't support VK_FORMAT_B8G8R8A8_SRGB.\n");
		exit(-1);
	}

	uint32_t present_mode_count;
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(vgd.physical_device, surface, &present_mode_count, nullptr) != VK_SUCCESS) {
		printf("Getting present modes count failed.\n");
		exit(-1);
	}

	std::vector<VkPresentModeKHR> present_modes;
	present_modes.resize(present_mode_count);
	if (vkGetPhysicalDeviceSurfacePresentModesKHR(vgd.physical_device, surface, &present_mode_count, present_modes.data()) != VK_SUCCESS) {
		printf("Getting present modes failed.\n");
		exit(-1);
	}

	for (uint32_t i = 0; i < present_mode_count; i++) {
		VkPresentModeKHR mode = present_modes[i];

	}

	VkSwapchainCreateInfoKHR swapchain_info = {};
	swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_info.surface = surface;
	swapchain_info.minImageCount = surface_capabilities.minImageCount;
	swapchain_info.imageFormat = preferred_swapchain_format;
	swapchain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchain_info.imageExtent = surface_capabilities.currentExtent;
	swapchain_info.imageArrayLayers = 1;
	swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_info.queueFamilyIndexCount = 1;
	swapchain_info.pQueueFamilyIndices = &vgd.graphics_queue_family_idx;
	swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	swapchain_info.clipped = VK_TRUE;

	if (vkCreateSwapchainKHR(vgd.device, &swapchain_info, vgd.alloc_callbacks, &swapchain) != VK_SUCCESS) {
		printf("Swapchain creation failed.\n");
		exit(-1);
	}
	printf("Created swapchain.\n");

	uint32_t swapchain_image_count;
	if (vkGetSwapchainImagesKHR(vgd.device, swapchain, &swapchain_image_count, nullptr) != VK_SUCCESS) {
		printf("Getting swapchain images count failed.\n");
		exit(-1);
	}

	swapchain_images.resize(swapchain_image_count);
	if (vkGetSwapchainImagesKHR(vgd.device, swapchain, &swapchain_image_count, swapchain_images.data()) != VK_SUCCESS) {
		printf("Getting swapchain images failed.\n");
		exit(-1);
	}

	//Create swapchain image view
	swapchain_image_views.resize(swapchain_image_count);
	{
		for (uint32_t i = 0; i < swapchain_image_count; i++) {
			VkImageSubresourceRange subresource_range = {};
			subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresource_range.baseMipLevel = 0;
			subresource_range.levelCount = 1;
			subresource_range.baseArrayLayer = 0;
			subresource_range.layerCount = 1;

			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = swapchain_images[i];
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = VK_FORMAT_B8G8R8A8_SRGB;
			info.components = COMPONENT_MAPPING_DEFAULT;
			info.subresourceRange = subresource_range;

			if (vkCreateImageView(vgd.device, &info, vgd.alloc_callbacks, &swapchain_image_views[i]) != VK_SUCCESS) {
				printf("Creating swapchain image view %i failed.\n", i);
				exit(-1);
			}
		}
	}

	//Create the acquire semaphore
	{
		VkSemaphoreCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(vgd.device, &info, vgd.alloc_callbacks, &semaphore) != VK_SUCCESS) {
			printf("Creating swapchain acquire semaphore failed.\n");
			exit(-1);
		}
	}

	//Create the present semaphore
	{
		VkSemaphoreCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(vgd.device, &info, vgd.alloc_callbacks, &present_semaphore) != VK_SUCCESS) {
			printf("Creating swapchain acquire semaphore failed.\n");
			exit(-1);
		}
	}
}