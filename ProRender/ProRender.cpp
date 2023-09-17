// ProRender.cpp : Defines the entry point for the application.
//

#include <chrono>
#include "ProRender.h"

constexpr uint64_t U64_MAX = std::numeric_limits<uint64_t>::max();

int main(int argc, char* argv[]) {
	printf("Argument 0: %s\n", argv[0]);

	//printf("current dir: %s\n", get_current_dir_name());

	SDL_Init(SDL_INIT_VIDEO);	//Initialize SDL

	const uint32_t x_resolution = 720;
	const uint32_t y_resolution = 720;
	SDL_Window* sdl_window = SDL_CreateWindow("losing my mind", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, x_resolution, y_resolution, SDL_WINDOW_VULKAN);

	//Init vulkan graphics device
	VulkanGraphicsDevice vgd;
	vgd.init();

	//Init the vulkan window
	VulkanWindow window;
	{
		VkSurfaceKHR surface;
		if (SDL_Vulkan_CreateSurface(sdl_window, vgd.instance, &surface) == SDL_FALSE) {
			printf("Creating VkSurface failed.\n");
			exit(-1);
		}

		window.init(vgd, surface);
	}

	//Render pass object creation
	VkRenderPass render_pass;
	{
		VkAttachmentDescription color_attachment = {
			.format = window.preferred_swapchain_format,
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
	swapchain_framebuffers.resize(window.swapchain_images.size());
	{
		for (uint32_t i = 0; i < swapchain_framebuffers.size(); i++) {
			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = render_pass;
			info.attachmentCount = 1;
			info.pAttachments = &window.swapchain_image_views[i];
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
		VkShaderModule vertex_shader = vgd.load_shader_module("shaders/test.vert.spv");
		VkShaderModule fragment_shader = vgd.load_shader_module("shaders/test.frag.spv");
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

	//Create graphics pipeline timeline semaphore
	VkSemaphore graphics_timeline_semaphore;
	{
		VkSemaphoreTypeCreateInfo type_info = {};
		type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		type_info.initialValue = 0;

		VkSemaphoreCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		info.pNext = &type_info;

		if (vkCreateSemaphore(vgd.device, &info, vgd.alloc_callbacks, &graphics_timeline_semaphore) != VK_SUCCESS) {
			printf("Creating graphics timeline semaphore failed.\n");
			exit(-1);
		}
	}

	//Main loop
	uint64_t ticks = SDL_GetTicks64();
	bool running = true;
	uint64_t current_frame = 0;
	while (running) {
		uint64_t tick_delta = SDL_GetTicks64() - ticks;
		ticks = SDL_GetTicks64();

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
			vkAcquireNextImageKHR(vgd.device, window.swapchain, U64_MAX, window.semaphore, VK_NULL_HANDLE, &acquired_image_idx);
			
			if (current_frame >= FRAMES_IN_FLIGHT) {
				uint64_t wait_value = current_frame - FRAMES_IN_FLIGHT + 1;
				VkSemaphoreWaitInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
				info.semaphoreCount = 1;
				info.pSemaphores = &graphics_timeline_semaphore;
				info.pValues = &wait_value;

				if (vkWaitSemaphores(vgd.device, &info, U64_MAX) != VK_SUCCESS) {
					printf("Waiting for graphics timeline semaphore failed.\n");
					exit(-1);
				}
			}

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

			float time = static_cast<float>(ticks) * 1.5 / 1000.0;
			vkCmdPushConstants(current_cb, vgd.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 4, &time);
			vkCmdDraw(current_cb, 3, 1, 0, 0);

			vkCmdEndRenderPass(current_cb);
			vkEndCommandBuffer(current_cb);

			//Submit rendering command buffer
			VkQueue q;
			vkGetDeviceQueue(vgd.device, vgd.graphics_queue_family_idx, 0, &q);
			{
				uint64_t wait_values[] = {current_frame , 0};
				uint64_t signal_values[] = {current_frame + 1 , 0};
				VkTimelineSemaphoreSubmitInfo ts_info = {};
				ts_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
				ts_info.waitSemaphoreValueCount = 2;
				ts_info.pWaitSemaphoreValues = wait_values;
				ts_info.signalSemaphoreValueCount = 2;
				ts_info.pSignalSemaphoreValues = signal_values;

				VkPipelineStageFlags flags[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
				VkSubmitInfo info = {};
				VkSemaphore semaphores[] = { graphics_timeline_semaphore, window.semaphore };
				info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				info.pNext = &ts_info;
				info.waitSemaphoreCount = 2;
				info.pWaitSemaphores = semaphores;
				info.signalSemaphoreCount = 2;
				info.pSignalSemaphores = semaphores;
				info.commandBufferCount = 1;
				info.pCommandBuffers = &current_cb;
				info.pWaitDstStageMask = flags;

				if (vkQueueSubmit(q, 1, &info, VK_NULL_HANDLE) != VK_SUCCESS) {
					printf("Queue submit failed.\n");
					exit(-1);
				}
			}

			//Queue present
			{
				VkPresentInfoKHR info = {};
				info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
				info.waitSemaphoreCount = 1;
				info.pWaitSemaphores = &window.semaphore;
				info.swapchainCount = 1;
				info.pSwapchains = &window.swapchain;
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
	vkDestroySemaphore(vgd.device, window.semaphore, vgd.alloc_callbacks);
	for (uint32_t i = 0; i < swapchain_framebuffers.size(); i++) {
		vkDestroyFramebuffer(vgd.device, swapchain_framebuffers[i], vgd.alloc_callbacks);
	}
	for (uint32_t i = 0; i < window.swapchain_image_views.size(); i++) {
		vkDestroyImageView(vgd.device, window.swapchain_image_views[i], vgd.alloc_callbacks);
	}
	vkDestroySwapchainKHR(vgd.device, window.swapchain, vgd.alloc_callbacks);
	vkDestroySurfaceKHR(vgd.instance, window.surface, vgd.alloc_callbacks);
	
	vkDestroySemaphore(vgd.device, graphics_timeline_semaphore, vgd.alloc_callbacks);
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
