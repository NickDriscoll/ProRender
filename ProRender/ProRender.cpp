﻿// ProRender.cpp : Defines the entry point for the application.
//

#include "ProRender.h"

constexpr uint64_t U64_MAX = std::numeric_limits<uint64_t>::max();

VkShaderModule load_shader_module(VulkanGraphicsDevice& vgd, const char* path) {
	//Get filesize
	uint32_t spv_size = std::filesystem::file_size(std::filesystem::path(path));

	//Allocate receiving buffer
	std::vector<uint32_t> spv_bytes;
	spv_bytes.resize(spv_size);

	//Read spv bytes
	FILE* shader_spv = fopen(path, "rb");
	if (fread(spv_bytes.data(), sizeof(uint8_t), spv_size, shader_spv) == 0) {
		printf("Zero bytes read when trying to read %s\n", path);
		exit(-1);
	}
	fclose(shader_spv);

	//Create shader module
	VkShaderModule shader;
	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = spv_size;
	info.pCode = spv_bytes.data();
	if (vkCreateShaderModule(vgd.device, &info, vgd.alloc_callbacks, &shader) != VK_SUCCESS) {
		printf("Creating vertex shader module failed.\n");
		exit(-1);
	}

	return shader;
}

int main(int argc, char* argv[]) {
	printf("Argument 0: %s\n", argv[0]);
	
	//printf("current dir: %s\n", get_current_dir_name());

	SDL_Init(SDL_INIT_VIDEO);	//Initialize SDL

	const uint32_t x_resolution = 720;
	const uint32_t y_resolution = 720;
	SDL_Window* window = SDL_CreateWindow("losing my mind", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, x_resolution, y_resolution, SDL_WINDOW_VULKAN);

	//Init vulkan graphics device
	VulkanGraphicsDevice vgd;
	vgd.init();

	//VkSurface and swapchain creation
	VkFormat preferred_swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;	//This seems to be a pretty standard/common swapchain format
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	VkSemaphore acquire_semaphore;
	std::vector<VkImage> swapchain_images;
	std::vector<VkImageView> swapchain_image_views;
	{
		if (SDL_Vulkan_CreateSurface(window, vgd.instance, &surface) == SDL_FALSE) {
			printf("Creating VkSurface failed.\n");
			exit(-1);
		}
		printf("Created surface.\n");

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
		swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
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
				VkComponentMapping mapping;
				mapping.r = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_R;
				mapping.g = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_G;
				mapping.b = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_B;
				mapping.a = VkComponentSwizzle::VK_COMPONENT_SWIZZLE_A;

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
				info.components = mapping;
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
			if (vkCreateSemaphore(vgd.device, &info, vgd.alloc_callbacks, &acquire_semaphore) != VK_SUCCESS) {
				printf("Creating swapchain acquire semaphore failed.\n");
				exit(-1);
			}
		}
	}


	//Render pass object creation
	VkRenderPass render_pass;
	{
		VkAttachmentDescription color_attachment = {
			.format = preferred_swapchain_format,
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

		VkRenderPassCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		info.attachmentCount = 1;
		info.pAttachments = &color_attachment;
		info.subpassCount = 1;
		info.pSubpasses = &subpass;

		if (vkCreateRenderPass(vgd.device, &info, vgd.alloc_callbacks, &render_pass) != VK_SUCCESS) {
			printf("Creating render pass failed.\n");
			exit(-1);
		}
	}

	//Create swapchain framebuffers
	std::vector<VkFramebuffer> swapchain_framebuffers;
	swapchain_framebuffers.resize(swapchain_images.size());
	{
		for (uint32_t i = 0; i < swapchain_framebuffers.size(); i++) {
			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = render_pass;
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

	//Create pipeline
	VkPipeline main_pipeline;
	{
		//Shader stages
		VkShaderModule vertex_shader = load_shader_module(vgd, "shaders/test.vert.spv");
		VkShaderModule fragment_shader = load_shader_module(vgd, "shaders/test.frag.spv");
		std::vector<VkPipelineShaderStageCreateInfo> shader_stage_creates;
		{

			VkPipelineShaderStageCreateInfo vert_info = {};
			vert_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vert_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vert_info.module = vertex_shader;
			vert_info.pName = "main";
			shader_stage_creates.push_back(vert_info);

			VkPipelineShaderStageCreateInfo frag_info = {};
			frag_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			frag_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			frag_info.module = fragment_shader;
			frag_info.pName = "main";
			shader_stage_creates.push_back(frag_info);
		}

		//Vertex input state
		//Doing vertex pulling so this can be mostly null :)
		VkPipelineVertexInputStateCreateInfo vert_input_info = {};
		vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;


		//Input assembly state
		VkPipelineInputAssemblyStateCreateInfo ia_info = {};
		ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		//Tesselation state
		VkPipelineTessellationStateCreateInfo tess_info = {};
		tess_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

		//Viewport and scissor state
		//These will _always_ be dynamic pipeline states
		VkPipelineViewportStateCreateInfo viewport_info = {};
		viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_info.viewportCount = 1;
		viewport_info.scissorCount = 1;

		//Rasterization state info
		VkPipelineRasterizationStateCreateInfo rast_info = {};
		rast_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rast_info.depthClampEnable = VK_FALSE;
		rast_info.rasterizerDiscardEnable = VK_FALSE;
		rast_info.polygonMode = VK_POLYGON_MODE_FILL;
		rast_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rast_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rast_info.depthBiasEnable = VK_FALSE;
		rast_info.lineWidth = 1.0;

		//Multisample state
		VkPipelineMultisampleStateCreateInfo multi_info = {};
		multi_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multi_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multi_info.sampleShadingEnable = VK_FALSE;

		//Depth stencil state
		VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {};
		depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_info.depthTestEnable = VK_TRUE;
		depth_stencil_info.depthWriteEnable = VK_TRUE;
		depth_stencil_info.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
		depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_info.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo blend_info = {};
		VkPipelineColorBlendAttachmentState blend_state = {};
		{
			//Blend func description
			VkColorComponentFlags component_flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			blend_state.blendEnable = VK_TRUE;
			blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
			blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
			blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
			blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
			blend_state.colorWriteMask = component_flags;


			//Blend state
			blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			blend_info.attachmentCount = 1;
			blend_info.pAttachments = &blend_state;
			blend_info.blendConstants[0] = 1.0;
			blend_info.blendConstants[1] = 1.0;
			blend_info.blendConstants[2] = 1.0;
			blend_info.blendConstants[3] = 1.0;
		}

		//Dynamic state info
		VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamic_info = {};
		dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_info.dynamicStateCount = 2;
		dynamic_info.pDynamicStates = dyn_states;

		VkGraphicsPipelineCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		info.stageCount = shader_stage_creates.size();
		info.pStages = shader_stage_creates.data();
		info.pVertexInputState = &vert_input_info;
		info.pInputAssemblyState = &ia_info;
		info.pTessellationState = &tess_info;
		info.pViewportState = &viewport_info;
		info.pRasterizationState = &rast_info;
		info.pMultisampleState = &multi_info;
		info.pDepthStencilState = &depth_stencil_info;
		info.pColorBlendState = &blend_info;
		info.pDynamicState = &dynamic_info;
		info.layout = vgd.pipeline_layout;
		info.renderPass = render_pass;
		info.subpass = 0;


		if (vkCreateGraphicsPipelines(vgd.device, vgd.pipeline_cache, 1, &info, vgd.alloc_callbacks, &main_pipeline) != VK_SUCCESS) {
			printf("Creating graphics pipeline.\n");
			exit(-1);
		}

		vkDestroyShaderModule(vgd.device, vertex_shader, vgd.alloc_callbacks);
		vkDestroyShaderModule(vgd.device, fragment_shader, vgd.alloc_callbacks);
	}
	printf("Created graphics pipeline.\n");

	//Create some fences
	VkFence render_fence;
	{
		VkFenceCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		if (vkCreateFence(vgd.device, &info, vgd.alloc_callbacks, &render_fence) != VK_SUCCESS) {
			printf("Creating presentation fence failed.\n");
			exit(-1);
		}
	}

	//Main loop
	bool running = true;
	uint64_t current_frame = 0;
	while (running) {
		//Do input polling loop
		SDL_Event current_event;
		while (SDL_PollEvent(&current_event)) {
			switch (current_event.type) {
			case SDL_QUIT:
				running = false;
				break;
			}
		}

		//Draw triangle
		{
			//Acquire swapchain image for this frame
			uint32_t acquired_image_idx;
			vkAcquireNextImageKHR(vgd.device, swapchain, U64_MAX, acquire_semaphore, VK_NULL_HANDLE, &acquired_image_idx);

			//Wait on previous rendering fence
			if (vkWaitForFences(vgd.device, 1, &render_fence, VK_TRUE, U64_MAX) != VK_SUCCESS) {
				printf("Waiting for present fence failed.\n");
				exit(-1);
			}
			vkResetFences(vgd.device, 1, &render_fence);

			VkCommandBuffer current_cb = vgd.command_buffers[current_frame % FRAMES_IN_FLIGHT];

			VkCommandBufferBeginInfo begin_info = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			vkBeginCommandBuffer(current_cb, &begin_info);

			vkCmdBindPipeline(current_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, main_pipeline);

			//Begin render pass
			{
				VkRect2D area = {
					.offset = {
						.x = 0,
						.y = 0
					},
					.extent = {
						.width = x_resolution,
						.height = y_resolution
					}
				};

				VkClearValue clear_color;
				clear_color.color.float32[0] = 0.0;
				clear_color.color.float32[1] = 0.0;
				clear_color.color.float32[2] = 0.0;
				clear_color.color.float32[3] = 1.0;
				clear_color.depthStencil.depth = 0.0;
				clear_color.depthStencil.stencil = 0;

				VkRenderPassBeginInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				info.renderPass = render_pass;
				info.framebuffer = swapchain_framebuffers[acquired_image_idx];
				info.renderArea = area;
				info.clearValueCount = 1;
				info.pClearValues = &clear_color;

				vkCmdBeginRenderPass(current_cb, &info, VK_SUBPASS_CONTENTS_INLINE);
			}

			//Set viewport and scissor
			{
				VkViewport viewport = {
					.x = 0,
					.y = 0,
					.width = x_resolution,
					.height = y_resolution,
					.minDepth = 0.0,
					.maxDepth = 1.0
				};
				vkCmdSetViewport(current_cb, 0, 1, &viewport);

				VkRect2D scissor = {
					.offset = {
						.x = 0,
						.y = 0
					},
					.extent = {
						.width = x_resolution,
						.height = y_resolution
					}
				};
				vkCmdSetScissor(current_cb, 0, 1, &scissor);
			}

			vkCmdDraw(current_cb, 3, 1, 0, 0);

			vkCmdEndRenderPass(current_cb);
			vkEndCommandBuffer(current_cb);

			//Submit rendering command buffer
			VkQueue q;
			vkGetDeviceQueue(vgd.device, vgd.graphics_queue_family_idx, 0, &q);
			{
				VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				VkSubmitInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				info.waitSemaphoreCount = 1;
				info.pWaitSemaphores = &acquire_semaphore;
				info.signalSemaphoreCount = 1;
				info.pSignalSemaphores = &acquire_semaphore;
				info.commandBufferCount = 1;
				info.pCommandBuffers = &current_cb;
				info.pWaitDstStageMask = &flags;

				if (vkQueueSubmit(q, 1, &info, render_fence) != VK_SUCCESS) {
					printf("Queue submit failed.\n");
					exit(-1);
				}
			}

			//Queue present
			{
				VkPresentInfoKHR info = {};
				info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
				info.waitSemaphoreCount = 1;
				info.pWaitSemaphores = &acquire_semaphore;
				info.swapchainCount = 1;
				info.pSwapchains = &swapchain;
				info.pImageIndices = &acquired_image_idx;
				info.pResults = VK_NULL_HANDLE;

				if (vkQueuePresentKHR(q, &info) != VK_SUCCESS) {
					printf("Queue present failed.\n");
					exit(-1);
				}
			}
		}
		current_frame++;
	}

	//Wait until all GPU queues have drained before cleaning up resources
	vkDeviceWaitIdle(vgd.device);

	//Cleanup resources
	vkDestroySemaphore(vgd.device, acquire_semaphore, vgd.alloc_callbacks);
	vkDestroyFence(vgd.device, render_fence, vgd.alloc_callbacks);

	for (uint32_t i = 0; i < swapchain_framebuffers.size(); i++) {
		vkDestroyFramebuffer(vgd.device, swapchain_framebuffers[i], vgd.alloc_callbacks);
	}
	for (uint32_t i = 0; i < swapchain_image_views.size(); i++) {
		vkDestroyImageView(vgd.device, swapchain_image_views[i], vgd.alloc_callbacks);
	}
	vkDestroySwapchainKHR(vgd.device, swapchain, vgd.alloc_callbacks);
	vkDestroySurfaceKHR(vgd.instance, surface, vgd.alloc_callbacks);
	
	vkDestroyCommandPool(vgd.device, vgd.command_pool, vgd.alloc_callbacks);
	vkDestroyPipeline(vgd.device, main_pipeline, vgd.alloc_callbacks);
	vkDestroyRenderPass(vgd.device, render_pass, vgd.alloc_callbacks);
	vkDestroyPipelineLayout(vgd.device, vgd.pipeline_layout, vgd.alloc_callbacks);
	vkDestroyDescriptorSetLayout(vgd.device, vgd.descriptor_set_layout, vgd.alloc_callbacks);
	vkDestroyPipelineCache(vgd.device, vgd.pipeline_cache, vgd.alloc_callbacks);
	vkDestroyDevice(vgd.device, vgd.alloc_callbacks);
	vkDestroyInstance(vgd.instance, vgd.alloc_callbacks);
	
	SDL_Quit();

	return 0;
}
