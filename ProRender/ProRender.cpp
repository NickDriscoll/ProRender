#include "ProRender.h"

int main(int argc, char* argv[]) {
	Timer app_timer;
	app_timer.start();

	SDL_Init(SDL_INIT_VIDEO);	//Initialize SDL

	const uint32_t x_resolution = 720;
	const uint32_t y_resolution = 720;
	SDL_Window* sdl_window = SDL_CreateWindow("losing my mind", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, x_resolution, y_resolution, SDL_WINDOW_VULKAN);

	//Init vulkan graphics device
	VulkanGraphicsDevice vgd = VulkanGraphicsDevice();

	//Init the vulkan window
	VkSurfaceKHR window_surface;
	if (SDL_Vulkan_CreateSurface(sdl_window, vgd.instance, &window_surface) == SDL_FALSE) {
		printf("Creating VkSurface failed.\n");
		exit(-1);
	}
	VulkanWindow window = VulkanWindow(vgd, window_surface);

	//Render pass object creation
	uint64_t render_pass_id;
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

		VkRenderPassCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &color_attachment,
			.subpassCount = 1,
			.pSubpasses = &subpass
		};
		
		render_pass_id = vgd.create_render_pass(info);
	}

	//Create swapchain framebuffers
	std::vector<VkFramebuffer> swapchain_framebuffers;
	swapchain_framebuffers.resize(window.swapchain_images.size());
	{
		for (uint32_t i = 0; i < swapchain_framebuffers.size(); i++) {
			VkFramebufferCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			info.renderPass = *vgd.get_render_pass(render_pass_id);
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

	//Create graphics pipelines
	uint64_t current_pipeline_handle = 0;
	uint64_t pipeline_handles[] = {0, 0};
	uint64_t normal_pipeline_handle;
	uint64_t wire_pipeline_handle;
	{
		VulkanInputAssemblyState ia_states[] = {
			{},
			{}
		};

		VulkanTesselationState tess_states[] = {
			{},
			{}
		};

		VulkanViewportState vs_states[] = {
			{},
			{}
		};

		VulkanRasterizationState rast_states[] = {
			{},
			{
				.polygonMode = VK_POLYGON_MODE_LINE
			}
		};

		VulkanMultisampleState ms_states[] = {
			{},
			{}
		};

		VulkanDepthStencilState ds_states[] = {
			{},
			{}
		};
		
		//Blend func description
		VkColorComponentFlags component_flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VulkanColorBlendAttachmentState blend_attachment_state = {};
		VulkanColorBlendState blend_states[] = {
			{
				.attachmentCount = 1,
				.pAttachments = &blend_attachment_state
			},
			{
				.attachmentCount = 1,
				.pAttachments = &blend_attachment_state
			}
		};

		const char* shaders[] = { "shaders/test.vert.spv", "shaders/test.frag.spv", "shaders/test.vert.spv", "shaders/test.frag.spv" };

		vgd.create_graphics_pipelines(
			render_pass_id,
			shaders,
			ia_states,
			tess_states,
			vs_states,
			rast_states,
			ms_states,
			ds_states,
			blend_states,
			pipeline_handles,
			2
		);
		normal_pipeline_handle = pipeline_handles[0];
		wire_pipeline_handle = pipeline_handles[1];
	}
	printf("Created graphics pipeline.\n");

	//Create graphics pipeline timeline semaphore
	VkSemaphore graphics_timeline_semaphore = vgd.create_timeline_semaphore(0);

	//Loading four images in a batch
	uint64_t image_batch_id;
	{
		const char* filenames[] = {
			"images/statue.jpg",
			"images/birds-allowed.png",
			"images/stressed_miyamoto.png",
			"images/normal.png"
		};
		VkFormat formats[] = {
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_FORMAT_R8G8B8A8_UNORM
		};
		image_batch_id = vgd.load_images(
			4,
			filenames,
			formats
		);
	}

	printf("Initialization time took %fms.\n", app_timer.check());
	
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
			case SDL_KEYDOWN:
				if (current_event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
					if (current_pipeline_handle == normal_pipeline_handle) {
						current_pipeline_handle = wire_pipeline_handle;
					} else {
						current_pipeline_handle = normal_pipeline_handle;
					}
				}
				break;
			}
		}

		//Draw
		{
			//Acquire swapchain image for this frame
			uint32_t acquired_image_idx;
			vkAcquireNextImageKHR(vgd.device, window.swapchain, U64_MAX, window.acquire_semaphore, VK_NULL_HANDLE, &acquired_image_idx);

			std::vector<VkSemaphore> wait_semaphores = {window.acquire_semaphore};
			std::vector<uint64_t> wait_semaphore_values = {0};
			std::vector<VkPipelineStageFlags> wait_flags = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };
			VkCommandBuffer current_cb = vgd.command_buffers[current_frame % FRAMES_IN_FLIGHT];
			
			//Wait for command buffer to finish execution before trying to record to it
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

			VkCommandBufferBeginInfo begin_info = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			vkBeginCommandBuffer(current_cb, &begin_info);

			//Per-frame checking of pending images to see if they're ready
			uint64_t upload_semaphore_value = vgd.tick_image_uploads(current_cb, wait_semaphores, wait_semaphore_values);

			vkCmdBindPipeline(current_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vgd.get_graphics_pipeline(current_pipeline_handle)->pipeline);
			vkCmdBindDescriptorSets(current_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vgd.pipeline_layout, 0, 1, &vgd.descriptor_set, 0, nullptr);

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
				clear_color.color.float32[2] = 1.0;
				clear_color.color.float32[3] = 1.0;
				clear_color.depthStencil.depth = 0.0;
				clear_color.depthStencil.stencil = 0;

				VkRenderPassBeginInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				info.renderPass = *vgd.get_render_pass(render_pass_id);
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

			float time = static_cast<float>(ticks) * 1.5f / 1000.0f;

			for (uint32_t i = 0; i < 4; i++) {
				uint32_t x = i & 1;
				uint32_t y = i > 1;
				uint32_t bytes[] = { std::bit_cast<uint32_t>(time), i, x, y };
				vkCmdPushConstants(current_cb, vgd.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, bytes);

				if (upload_semaphore_value >= image_batch_id) {
					vkCmdDraw(current_cb, 6, 1, 0, 0);
				}
			}

			vkCmdEndRenderPass(current_cb);
			vkEndCommandBuffer(current_cb);

			//Submit rendering command buffer
			VkQueue q;
			vkGetDeviceQueue(vgd.device, vgd.graphics_queue_family_idx, 0, &q);
			{
				uint64_t signal_values[] = {current_frame + 1, 0};
				VkTimelineSemaphoreSubmitInfo ts_info = {};
				ts_info.waitSemaphoreValueCount = wait_semaphore_values.size();
				ts_info.pWaitSemaphoreValues = wait_semaphore_values.data();
				ts_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
				ts_info.signalSemaphoreValueCount = 2;
				ts_info.pSignalSemaphoreValues = signal_values;

				VkSubmitInfo info = {};
				VkSemaphore signal_semaphores[] = { graphics_timeline_semaphore, window.present_semaphore };
				info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				info.pNext = &ts_info;
				info.waitSemaphoreCount = wait_semaphores.size();
				info.pWaitSemaphores = wait_semaphores.data();
				info.pWaitDstStageMask = wait_flags.data();
				info.signalSemaphoreCount = 2;
				info.pSignalSemaphores = signal_semaphores;
				info.commandBufferCount = 1;
				info.pCommandBuffers = &current_cb;

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
				info.pWaitSemaphores = &window.present_semaphore;
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

	//vkDestroyImageView(vgd.device, sampled_image_view, vgd.alloc_callbacks);
	//vmaDestroyImage(vgd.allocator, sampled_image, image_allocation);
	vkDestroySemaphore(vgd.device, graphics_timeline_semaphore, vgd.alloc_callbacks);

	vkDestroyPipeline(vgd.device, vgd.get_graphics_pipeline(normal_pipeline_handle)->pipeline, vgd.alloc_callbacks);
	vkDestroyPipeline(vgd.device, vgd.get_graphics_pipeline(wire_pipeline_handle)->pipeline, vgd.alloc_callbacks);

	for (uint32_t i = 0; i < swapchain_framebuffers.size(); i++) {
		vkDestroyFramebuffer(vgd.device, swapchain_framebuffers[i], vgd.alloc_callbacks);
	}
	
	SDL_Quit();

	return 0;
}

