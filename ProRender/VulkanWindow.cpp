#include "VulkanWindow.h"

VulkanWindow::VulkanWindow(VulkanGraphicsDevice& vgd, VkSurfaceKHR surface) {
    //Save surface
    this->surface = surface;

	instance = vgd.instance;
	device = vgd.device;
	alloc_callbacks = vgd.alloc_callbacks;
	preferred_present_mode = VK_PRESENT_MODE_FIFO_KHR;

    //Query for surface capabilities
	VkSurfaceCapabilitiesKHR surface_capabilities = {};
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vgd.physical_device, surface, &surface_capabilities) != VK_SUCCESS) {
		printf("Getting VkSurface capabilities failed.\n");
		exit(-1);
	}
	this->x_resolution = surface_capabilities.currentExtent.width;
	this->y_resolution = surface_capabilities.currentExtent.height;

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

	VkFormat preferred_swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;
	for (uint32_t i = 0; i < format_count; i++) {
		VkSurfaceFormatKHR format = formats[i];
		if (format.format == preferred_swapchain_format) {
			this->format = format;
			break;
		}
	}
	if (format.format == 0) {
		printf("Surface doesn't support preferred format.\n");
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
	swapchain_info.imageFormat = format.format;
	swapchain_info.imageColorSpace = format.colorSpace;
	swapchain_info.imageExtent = surface_capabilities.currentExtent;
	swapchain_info.imageArrayLayers = 1;
	swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_info.queueFamilyIndexCount = 1;
	swapchain_info.pQueueFamilyIndices = &vgd.graphics_queue_family_idx;
	swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_info.presentMode = preferred_present_mode;
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
			info.format = format.format;
			info.components = COMPONENT_MAPPING_DEFAULT;
			info.subresourceRange = subresource_range;

			if (vkCreateImageView(vgd.device, &info, vgd.alloc_callbacks, &swapchain_image_views[i]) != VK_SUCCESS) {
				printf("Creating swapchain image view %i failed.\n", i);
				exit(-1);
			}
		}
	}

	//Render pass object creation
	{
		VkAttachmentDescription color_attachment = {
			.format = format.format,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};

		VkAttachmentReference attachment_ref = {
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
		};

		VkSubpassDescription subpass = {
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &attachment_ref
		};

		VkRenderPassCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &color_attachment,
			.subpassCount = 1,
			.pSubpasses = &subpass
		};
		
		swapchain_renderpass = vgd.create_render_pass(info);
	}

	//Create swapchain framebuffers
	{
		swapchain_framebuffers.resize(swapchain_images.size());
		for (uint32_t i = 0; i < swapchain_framebuffers.size(); i++) {
			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = *vgd.get_render_pass(swapchain_renderpass);
			info.attachmentCount = 1;
			info.pAttachments = &swapchain_image_views[i];
			info.width = x_resolution;
			info.height = y_resolution;
			info.layers = 1;

			if (vkCreateFramebuffer(vgd.device, &info, vgd.alloc_callbacks, &swapchain_framebuffers[i]) != VK_SUCCESS) {
				printf("Creating swapchain framebuffer %i failed.\n", i);
				exit(-1);
			}
		}
	}

	//Create the acquire semaphore
	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
		VkSemaphoreCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(vgd.device, &info, vgd.alloc_callbacks, &acquire_semaphores[i]) != VK_SUCCESS) {
			printf("Creating swapchain acquire semaphore failed.\n");
			exit(-1);
		}
	}

	//Create the present semaphore
	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
		VkSemaphoreCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		if (vkCreateSemaphore(vgd.device, &info, vgd.alloc_callbacks, &present_semaphores[i]) != VK_SUCCESS) {
			printf("Creating swapchain acquire semaphore failed.\n");
			exit(-1);
		}
	}
}

VulkanWindow::~VulkanWindow() {
	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
		vkDestroySemaphore(device, acquire_semaphores[i], alloc_callbacks);
		vkDestroySemaphore(device, present_semaphores[i], alloc_callbacks);
	}

	for (uint32_t i = 0; i < swapchain_framebuffers.size(); i++) {
		vkDestroyFramebuffer(device, swapchain_framebuffers[i], alloc_callbacks);
	}
	for (uint32_t i = 0; i < swapchain_image_views.size(); i++) {
		vkDestroyImageView(device, swapchain_image_views[i], alloc_callbacks);
	}
	vkDestroySwapchainKHR(device, swapchain, alloc_callbacks);
	vkDestroySurfaceKHR(instance, surface, alloc_callbacks);
}

void VulkanWindow::resize(VulkanGraphicsDevice& vgd) {
	
	vkDeviceWaitIdle(vgd.device);
	
	//Destroy old swapchain resources
	for (uint32_t i = 0; i < swapchain_framebuffers.size(); i++) {
		vkDestroyFramebuffer(device, swapchain_framebuffers[i], alloc_callbacks);
	}
	for (uint32_t i = 0; i < swapchain_image_views.size(); i++) {
		vkDestroyImageView(device, swapchain_image_views[i], alloc_callbacks);
	}

    //Query for surface capabilities
	VkSurfaceCapabilitiesKHR surface_capabilities = {};
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vgd.physical_device, surface, &surface_capabilities) != VK_SUCCESS) {
		printf("Getting VkSurface capabilities failed.\n");
		exit(-1);
	}

	this->x_resolution = surface_capabilities.currentExtent.width;
	this->y_resolution = surface_capabilities.currentExtent.height;

	VkSwapchainCreateInfoKHR swapchain_info = {};
	swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_info.surface = surface;
	swapchain_info.minImageCount = surface_capabilities.minImageCount;
	swapchain_info.imageFormat = format.format;
	swapchain_info.imageColorSpace = format.colorSpace;
	swapchain_info.imageExtent = surface_capabilities.currentExtent;
	swapchain_info.imageArrayLayers = 1;
	swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_info.queueFamilyIndexCount = 1;
	swapchain_info.pQueueFamilyIndices = &vgd.graphics_queue_family_idx;
	swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_info.presentMode = preferred_present_mode;
	swapchain_info.clipped = VK_TRUE;
	swapchain_info.oldSwapchain = swapchain;

	VkSwapchainKHR new_swapchain;
	if (vkCreateSwapchainKHR(vgd.device, &swapchain_info, vgd.alloc_callbacks, &new_swapchain) != VK_SUCCESS) {
		printf("Swapchain creation failed.\n");
		exit(-1);
	}
	swapchain = new_swapchain;
	vkDestroySwapchainKHR(device, swapchain_info.oldSwapchain, alloc_callbacks);

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
	swapchain_image_views.clear();
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
			info.format = format.format;
			info.components = COMPONENT_MAPPING_DEFAULT;
			info.subresourceRange = subresource_range;

			if (vkCreateImageView(vgd.device, &info, vgd.alloc_callbacks, &swapchain_image_views[i]) != VK_SUCCESS) {
				printf("Creating swapchain image view %i failed.\n", i);
				exit(-1);
			}
		}
	}

	//Recreate swapchain framebuffers
	{
		swapchain_framebuffers.clear();
		swapchain_framebuffers.resize(swapchain_images.size());
		for (uint32_t i = 0; i < swapchain_framebuffers.size(); i++) {
			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = *vgd.get_render_pass(swapchain_renderpass);
			info.attachmentCount = 1;
			info.pAttachments = &swapchain_image_views[i];
			info.width = x_resolution;
			info.height = y_resolution;
			info.layers = 1;

			if (vkCreateFramebuffer(vgd.device, &info, vgd.alloc_callbacks, &swapchain_framebuffers[i]) != VK_SUCCESS) {
				printf("Creating swapchain framebuffer %i failed.\n", i);
				exit(-1);
			}
		}
	}
}