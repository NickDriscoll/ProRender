#include "ProRender.h"

int main(int argc, char* argv[]) {
	Timer init_timer = Timer("Init");
	Timer app_timer = Timer("Main function");

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);	//Initialize SDL
	app_timer.print("SDL Initialization");
	app_timer.start();

	//Init vulkan graphics device
	VulkanGraphicsDevice vgd = VulkanGraphicsDevice();
	app_timer.print("VGD Initialization");
	app_timer.start();
	
	std::vector<uint32_t> image_indices;
	std::vector<uint64_t> batch_ids;
	uint64_t max_id_taken_care_of = 0;
	batch_ids.reserve(4);
	{
		std::vector<const char*> filenames = {
			"images/doogan.png",
			"images/birds-allowed.png"
		};
		std::vector<const char*> filenames2 = {
			"images/normal.png",
			"images/stressed_miyamoto.png"
		};
		std::vector<VkFormat> formats = {
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_FORMAT_R8G8B8A8_UNORM
		};
		std::vector<VkFormat> formats2 = {
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_FORMAT_R8G8B8A8_UNORM
		};

		uint32_t count = filenames.size();
		batch_ids.push_back(vgd.load_images(count, std::move(filenames), std::move(formats)));
		batch_ids.push_back(vgd.load_images(count, std::move(filenames2), std::move(formats2)));
	}

	//uint32_t x_resolution = 720;
	//uint32_t y_resolution = 720;
	uint32_t window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
	SDL_Window* sdl_window = SDL_CreateWindow("losing my mind", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 720, 720, window_flags);
	
	//Init the vulkan window
	VkSurfaceKHR window_surface;
	if (SDL_Vulkan_CreateSurface(sdl_window, vgd.instance, &window_surface) == SDL_FALSE) {
		printf("Creating VkSurface failed.\n");
		exit(-1);
	}
	VulkanWindow window(vgd, window_surface);
	app_timer.print("Window creation");
	app_timer.start();

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
			window.swapchain_renderpass,
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

	hlslpp::float4x4 projection_matrix = hlslpp::float4x4(
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0,
		0.0, 0.0, 0.0, 0.0
	);

	//Create graphics pipeline timeline semaphore
	VkSemaphore graphics_timeline_semaphore = vgd.create_timeline_semaphore(0);

	init_timer.print("App init");
	
	//Main loop
	bool running = true;
	uint64_t current_frame = 0;
	while (running) {
		Timer frame_timer;
		frame_timer.start();

		//Do input polling loop
		{
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				switch (event.type) {
				case SDL_QUIT:
					running = false;
					break;
				case SDL_WINDOWEVENT:
					switch (event.window.event) {
						case SDL_WINDOWEVENT_SIZE_CHANGED:
							window.resize(vgd);
							break;
					}
					break;
				case SDL_KEYDOWN:
					if (event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
						if (current_pipeline_handle == normal_pipeline_handle) {
							current_pipeline_handle = wire_pipeline_handle;
						} else {
							current_pipeline_handle = normal_pipeline_handle;
						}
					}
					break;
				}
			}
		}

		//Draw
		{
			//Acquire swapchain image for this frame
			uint32_t acquired_image_idx;
			vkAcquireNextImageKHR(vgd.device, window.swapchain, U64_MAX, window.acquire_semaphore, VK_NULL_HANDLE, &acquired_image_idx);

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
			vgd.tick_image_uploads(current_cb);
			uint64_t upload_batches_completed = vgd.get_completed_image_uploads();

			{
				for (uint32_t i = 0; i < batch_ids.size(); i++) {
					if (max_id_taken_care_of < batch_ids[i] && batch_ids[i] <= upload_batches_completed) {
						uint32_t seen = 0;
						for (uint32_t j = 0; seen < vgd.available_images.count(); j++) {
							if (!vgd.available_images.is_live(j)) continue;
							seen += 1;

							VulkanAvailableImage* image = vgd.available_images.data() + j;
							if (batch_ids[i] == image->batch_id) {
								printf("Found image from batch %i at index %i\n", i, j);
								image_indices.push_back(j);
								max_id_taken_care_of = batch_ids[i];
							}
						}
					}
				}
			}

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
						.width = window.x_resolution,
						.height = window.y_resolution
					}
				};

				VkClearValue clear_color;
				clear_color.color.float32[0] = 0.1;
				clear_color.color.float32[1] = 0.0;
				clear_color.color.float32[2] = 0.9;
				clear_color.color.float32[3] = 1.0;
				clear_color.depthStencil.depth = 0.0;
				clear_color.depthStencil.stencil = 0;

				VkRenderPassBeginInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				info.renderPass = *vgd.get_render_pass(window.swapchain_renderpass);
				info.framebuffer = window.swapchain_framebuffers[acquired_image_idx];
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
					.width = (float)window.x_resolution,
					.height = (float)window.y_resolution,
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
						.width = window.x_resolution,
						.height = window.y_resolution
					}
				};
				vkCmdSetScissor(current_cb, 0, 1, &scissor);
			}

			float time = app_timer.check() * 1.5f / 1000.0f;

			for (uint32_t i = 0; i < image_indices.size(); i++) {
				uint32_t x = i & 1;
				uint32_t y = i > 1;
				uint32_t bytes[] = { std::bit_cast<uint32_t>(time), image_indices[i], x, y };
				vkCmdPushConstants(current_cb, vgd.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, bytes);

				vkCmdDraw(current_cb, 6, 1, 0, 0);
			}

			vkCmdEndRenderPass(current_cb);
			vkEndCommandBuffer(current_cb);

			//Submit rendering command buffer
			VkQueue q;
			vkGetDeviceQueue(vgd.device, vgd.graphics_queue_family_idx, 0, &q);
			{
				uint64_t wait_values[] = {0, upload_batches_completed};
				uint64_t signal_values[] = {current_frame + 1, 0};
				VkTimelineSemaphoreSubmitInfo ts_info = {};
				ts_info.waitSemaphoreValueCount = 2;
				ts_info.pWaitSemaphoreValues = wait_values;
				ts_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
				ts_info.signalSemaphoreValueCount = 2;
				ts_info.pSignalSemaphoreValues = signal_values;

				VkPipelineStageFlags wait_flags[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };

				VkSubmitInfo info = {};
				VkSemaphore signal_semaphores[] = { graphics_timeline_semaphore, window.present_semaphore };
				VkSemaphore wait_semaphores[] = {window.acquire_semaphore, vgd.image_upload_semaphore};
				info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				info.pNext = &ts_info;
				info.waitSemaphoreCount = 2;
				info.pWaitSemaphores = wait_semaphores;
				info.pWaitDstStageMask = wait_flags;
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

				VkResult r = vkQueuePresentKHR(q, &info);
				switch (r) {
					case VK_SUBOPTIMAL_KHR:
						printf("Swapchain suboptimal.\n");
						break;
					case VK_ERROR_OUT_OF_DATE_KHR:
						printf("Swapchain out of date.\n");
						window.resize(vgd);
						break;
					case VK_SUCCESS:
						break;
					default:
						printf("Queue present failed.\n");
						exit(-1);
						break;
				}
			}
		}

		//End-of-frame bookkeeping
		current_frame++;
	}

	//Wait until all GPU queues have drained before cleaning up resources
	vkDeviceWaitIdle(vgd.device);

	//Cleanup resources

	vkDestroySemaphore(vgd.device, graphics_timeline_semaphore, vgd.alloc_callbacks);

	vkDestroyPipeline(vgd.device, vgd.get_graphics_pipeline(normal_pipeline_handle)->pipeline, vgd.alloc_callbacks);
	vkDestroyPipeline(vgd.device, vgd.get_graphics_pipeline(wire_pipeline_handle)->pipeline, vgd.alloc_callbacks);
	
	SDL_Quit();

	return 0;
}

