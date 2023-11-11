#include "ProRender.h"

int main(int argc, char* argv[]) {
	Timer init_timer = Timer("Init");
	Timer app_timer = Timer("Main function");

	Configuration my_config = {
		.window_width = 1200,
		.window_height = 900	
	};

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);	//Initialize SDL
	app_timer.print("SDL Initialization");
	app_timer.start();

	//Init vulkan graphics device
	VulkanGraphicsDevice vgd = VulkanGraphicsDevice();
	app_timer.print("VGD Initialization");
	app_timer.start();

	//Initialize Dear ImGui
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
    	ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(my_config.window_width, my_config.window_height);

		//Build and upload font atlas
		uint8_t* tex_pixels = nullptr;
		int tex_w, tex_h;
		io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);
		RawImage im = {
			.width = (uint32_t)tex_w,
			.height = (uint32_t)tex_h,
			.data = tex_pixels
		};

		std::vector<RawImage> images = std::vector<RawImage>{
			im
		};
		std::vector<VkFormat> formats = std::vector<VkFormat>{
			VK_FORMAT_R8G8B8A8_UNORM
		};
		uint64_t batch_id = vgd.load_raw_images(images, formats);

		VkSemaphoreWaitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
		info.semaphoreCount = 1;
		info.pSemaphores = &vgd.image_upload_semaphore;
		info.pValues = &batch_id;
		if (vkWaitSemaphores(vgd.device, &info, U64_MAX) != VK_SUCCESS) {
			printf("Waiting for graphics timeline semaphore failed.\n");
			exit(-1);
		}
	
		uint32_t tex_index = 0;
		uint32_t seen = 0;
		for (uint32_t j = 0; seen < vgd.available_images.count(); j++) {
			if (!vgd.available_images.is_live(j)) continue;
			seen += 1;

			VulkanAvailableImage* image = vgd.available_images.data() + j;
			if (batch_id == image->batch_id) {
				tex_index = j;
				break;
			}
		}

		io.Fonts->SetTexID((void*)tex_index);
	}
	app_timer.print("Dear ImGUI Initialization");
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

		batch_ids.push_back(vgd.load_image_files(std::move(filenames), std::move(formats)));
		batch_ids.push_back(vgd.load_image_files(std::move(filenames2), std::move(formats2)));
	}

	uint32_t window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
	SDL_Window* sdl_window = SDL_CreateWindow("losing my mind", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, my_config.window_width, my_config.window_height, window_flags);
	
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

	//Create Dear ImGUI pipeline
	uint64_t imgui_pipeline;
	{
		VulkanInputAssemblyState ia_states[] = {
			{}
		};

		VulkanTesselationState tess_states[] = {
			{}
		};

		VulkanViewportState vs_states[] = {
			{}
		};

		VulkanRasterizationState rast_states[] = {
			{
				.cullMode = VK_CULL_MODE_NONE
			}
		};

		VulkanMultisampleState ms_states[] = {
			{}
		};

		VulkanDepthStencilState ds_states[] = {
			{}
		};
		
		//Blend func description
		VkColorComponentFlags component_flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VulkanColorBlendAttachmentState blend_attachment_state = {};
		VulkanColorBlendState blend_states[] = {
			{
				.attachmentCount = 1,
				.pAttachments = &blend_attachment_state
			}
		};

		const char* shaders[] = { "shaders/imgui.vert.spv", "shaders/imgui.frag.spv" };

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
			&imgui_pipeline,
			1
		);
	}

	//Create graphics pipeline timeline semaphore
	VkSemaphore graphics_timeline_semaphore = vgd.create_timeline_semaphore(0);

	//Initialize the renderer
	Renderer renderer(&vgd);
    renderer.frame_uniforms.clip_from_screen = hlslpp::float4x4(
        2.0 / window.x_resolution, 0.0, 0.0, -1.0,
        0.0, 2.0 / window.y_resolution, 0.0, -1.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );

	init_timer.print("App init");
				
	printf("ImDrawVert size: %i bytes\n", sizeof(ImDrawVert));
	
	//Main loop
	bool running = true;
	uint64_t current_frame = 0;
	double last_frame_took = 0.0;
	while (running) {
		Timer frame_timer;
		frame_timer.start();

		//Do input polling loop
		{
    		ImGuiIO& io = ImGui::GetIO();
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
							io.DisplaySize = ImVec2(window.x_resolution, window.y_resolution);
							renderer.frame_uniforms.clip_from_screen = hlslpp::float4x4(
								2.0 / window.x_resolution, 0.0, 0.0, -1.0,
								0.0, 2.0 / window.y_resolution, 0.0, -1.0,
								0.0, 0.0, 1.0, 0.0,
								0.0, 0.0, 0.0, 1.0
							);
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
				case SDL_MOUSEMOTION:
					io.AddMousePosEvent(event.motion.x, event.motion.y);
					break;
				case SDL_MOUSEBUTTONDOWN:
					io.AddMouseButtonEvent(event.button.which, true);
					break;
				case SDL_MOUSEBUTTONUP:
					io.AddMouseButtonEvent(event.button.which, false);
					break;
				case SDL_MOUSEWHEEL:
					io.AddMouseWheelEvent(event.wheel.preciseX, event.wheel.preciseY);
					break;
				}
			}
		}

		//Dear ImGUI update part
		{
    		ImGuiIO& io = ImGui::GetIO();
			io.DeltaTime = last_frame_took;

			ImGui::NewFrame();
        	ImGui::ShowDemoWindow(nullptr);
		}

		//Update per-frame uniforms
		{
			VulkanBuffer* uniform_buffer = vgd.get_buffer(renderer.frame_uniforms_buffer);
			memcpy(uniform_buffer->alloc_info.pMappedData, &renderer.frame_uniforms, sizeof(FrameUniforms));
		}

		//Draw
		{
			//Acquire swapchain image for this frame
			uint32_t acquired_image_idx;
			vkAcquireNextImageKHR(vgd.device, window.swapchain, U64_MAX, window.acquire_semaphore, VK_NULL_HANDLE, &acquired_image_idx);

			VkCommandBuffer frame_cb = vgd.command_buffers[current_frame % FRAMES_IN_FLIGHT];
			
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
			vkBeginCommandBuffer(frame_cb, &begin_info);

			//Per-frame checking of pending images to see if they're ready
			vgd.tick_image_uploads(frame_cb);
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

			vkCmdBindDescriptorSets(frame_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vgd.pipeline_layout, 0, 1, &vgd.descriptor_set, 0, nullptr);
			vkCmdBindPipeline(frame_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vgd.get_graphics_pipeline(current_pipeline_handle)->pipeline);

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

				vkCmdBeginRenderPass(frame_cb, &info, VK_SUBPASS_CONTENTS_INLINE);
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
				vkCmdSetViewport(frame_cb, 0, 1, &viewport);

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
				vkCmdSetScissor(frame_cb, 0, 1, &scissor);
			}

			float time = app_timer.check() * 1.5f / 1000.0f;

			for (uint32_t i = 0; i < image_indices.size(); i++) {
				uint32_t x = i & 1;
				uint32_t y = i > 1;

				uint32_t idx = image_indices[i];

				uint32_t bytes[] = { std::bit_cast<uint32_t>(time), idx, x, y };
				vkCmdPushConstants(frame_cb, vgd.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16, bytes);

				vkCmdDraw(frame_cb, 6, 1, 0, 0);
			}

			//Upload ImGUI vertex data and record ImGUI draw commands
			{
				uint32_t frame_slot = current_frame % FRAMES_IN_FLIGHT;
				uint32_t last_frame_slot = frame_slot == 0 ? FRAMES_IN_FLIGHT - 1 : frame_slot - 1;
				ImguiFrame& current_frame = renderer.imgui_frames[frame_slot];
				ImguiFrame& last_frame = renderer.imgui_frames[last_frame_slot];

				ImGui::Render();
				ImDrawData* draw_data = ImGui::GetDrawData();

				const uint32_t per_vertex_padding = vgd.physical_limits.minStorageBufferOffsetAlignment - (sizeof(ImDrawVert) % vgd.physical_limits.minStorageBufferOffsetAlignment);
				uint32_t im_vertex_size = draw_data->TotalVtxCount * (sizeof(ImDrawVert) + per_vertex_padding);

				//TODO: This local offset logic might only hold when FRAMES_IN_FLIGHT == 2
				uint32_t vertex_local_offset = 0;
				if (last_frame.vertex_start < im_vertex_size) {
					vertex_local_offset = last_frame.vertex_start + last_frame.vertex_size;
				}
				current_frame.vertex_start = vertex_local_offset;
				current_frame.vertex_size = im_vertex_size;

				VulkanBuffer* im_vert_buffer = vgd.get_buffer(renderer.imgui_vertex_buffer);
				uint8_t* vertex_ptr = vertex_local_offset + reinterpret_cast<uint8_t*>(im_vert_buffer->alloc_info.pMappedData);
			
				uint32_t im_index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);

				//TODO: This local offset logic might only hold when FRAMES_IN_FLIGHT == 2
				uint32_t index_local_offset = 0;
				if (last_frame.index_start < im_index_size) {
					index_local_offset = last_frame.index_start + last_frame.index_size;
				}
				current_frame.index_start = index_local_offset;
				current_frame.index_size = im_index_size;

				VulkanBuffer* im_idx_buffer = vgd.get_buffer(renderer.imgui_index_buffer);
				uint8_t* index_ptr = index_local_offset + reinterpret_cast<uint8_t*>(im_idx_buffer->alloc_info.pMappedData);

				//Record once-per-frame binding of index buffer and graphics pipeline
				vkCmdBindIndexBuffer(frame_cb, im_idx_buffer->buffer, index_local_offset, VK_INDEX_TYPE_UINT16);
				vkCmdBindPipeline(frame_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vgd.get_graphics_pipeline(imgui_pipeline)->pipeline);

				//Create intermediate vertex buffer
				std::vector<uint8_t> vertex_buffer;
				vertex_buffer.resize(im_vertex_size);
				uint8_t* inter_vert_ptr = vertex_buffer.data();

				for (uint32_t i = 0; i < draw_data->CmdListsCount; i++) {
					ImDrawList* draw_list = draw_data->CmdLists[i];

					//Copy vertex data into intermediate buffer
					uint32_t imgui_vert_offset = 0;
					uint32_t inter_offset = 0;
					for (uint32_t j = 0; j < draw_list->VtxBuffer.Size; j++) {
						uint8_t* read = reinterpret_cast<uint8_t*>(draw_list->VtxBuffer.Data) + imgui_vert_offset;
						uint8_t* write = inter_vert_ptr + inter_offset;
						memcpy(write, read, sizeof(ImDrawVert));
						imgui_vert_offset += sizeof(ImDrawVert);
						inter_offset += sizeof(ImDrawVert) + per_vertex_padding;
					}
					inter_vert_ptr += draw_list->VtxBuffer.Size * (sizeof(ImDrawVert) + per_vertex_padding);

					//Copy index data
					size_t copy_size = draw_list->IdxBuffer.Size * sizeof(ImDrawIdx);
					memcpy(index_ptr, draw_list->IdxBuffer.Data, copy_size);
					index_ptr += copy_size;

					//Record draw commands
					for (uint32_t j = 0; j < draw_list->CmdBuffer.Size; j++){
						ImDrawCmd& draw_command = draw_list->CmdBuffer[j];

						VkRect2D scissor = {
							.offset = {
								.x = (int32_t)draw_command.ClipRect.x,
								.y = (int32_t)draw_command.ClipRect.y
							},
							.extent = {
								.width = (uint32_t)draw_command.ClipRect.z - (uint32_t)draw_command.ClipRect.x,
								.height = (uint32_t)draw_command.ClipRect.w - (uint32_t)draw_command.ClipRect.y
							}
						};
						vkCmdSetScissor(frame_cb, 0, 1, &scissor);

						vkCmdDrawIndexed(frame_cb, draw_command.ElemCount, 1, draw_command.IdxOffset, draw_command.VtxOffset + (vertex_local_offset / (sizeof(ImDrawVert) + per_vertex_padding)), 0);
					}
				}

				//Finally copy the intermediate vertex buffer to the real one on the GPU
				memcpy(vertex_ptr, vertex_buffer.data(), vertex_buffer.size());
			}

			vkCmdEndRenderPass(frame_cb);
			vkEndCommandBuffer(frame_cb);

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
				info.pCommandBuffers = &frame_cb;

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
		last_frame_took = frame_timer.check();
	}

	//Wait until all GPU queues have drained before cleaning up resources
	vkDeviceWaitIdle(vgd.device);

	//Cleanup resources

	vkDestroySemaphore(vgd.device, graphics_timeline_semaphore, vgd.alloc_callbacks);
	
    ImGui::DestroyContext();
	SDL_Quit();

	return 0;
}
