#include "ProRender.h"
#include "ImguiRenderer.h"
#include "tinyfiledialogs.h"

using namespace hlslpp;

int main(int argc, char* argv[]) {
	Timer init_timer = Timer("Init");
	Timer app_timer = Timer("Main function");

	Configuration my_config = {
		.window_width = 1600,
		.window_height = 1000	
	};

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);	//Initialize SDL
	app_timer.print("SDL Initialization");
	app_timer.start();

	//Init vulkan graphics device
	VulkanGraphicsDevice vgd = VulkanGraphicsDevice();
	app_timer.print("VGD Initialization");
	app_timer.start();

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

	//Initialize the renderer
	Renderer renderer(&vgd, window.swapchain_renderpass);
    renderer.frame_uniforms.clip_from_screen = float4x4(
        2.0f / (float)window.x_resolution, 0.0f, 0.0f, -1.0f,
        0.0f, 2.0f / (float)window.y_resolution, 0.0f, -1.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

	//Initialize Dear ImGui
	ImguiRenderer imgui_renderer = ImguiRenderer(
		&vgd,
		renderer.point_sampler_idx,
		ImVec2(window.x_resolution, window.y_resolution),
		renderer.pipeline_layout_id,
		window.swapchain_renderpass,
		renderer.descriptor_set
	);
	app_timer.print("Dear ImGUI Initialization");
	app_timer.start();

    //Create main camera
	uint64_t main_viewport_camera = renderer.cameras.insert({ .position = { 1.0, -2.0, 5.0 }, .pitch = 1.3 });
	bool camera_control = false;
	int32_t mouse_saved_x, mouse_saved_y;

	//Load simple 3D plane
	uint64_t plane_image_batch_id;
	uint32_t plane_image_idx = 0xFFFFFFFF;
	uint64_t plane_key;
	{
		//Load plane texture
		{
			std::vector<const char*> names = { "images/stressed-miyamoto.jpg" };
			std::vector<VkFormat> formats = { VK_FORMAT_R8G8B8A8_SRGB };
			plane_image_batch_id = vgd.load_image_files(names, formats);
		}

		float plane_pos[] = {
			-10.0, -10.0, 0.0, 1.0,
			10.0, -10.0, 0.0, 1.0,
			-10.0, 10.0, 0.0, 1.0,
			10.0, 10.0, 0.0, 1.0
		};
		float plane_uv[] = {
			0.0, 1.0,
			1.0, 1.0,
			0.0, 0.0,
			1.0, 0.0
		};
		uint16_t inds[] = {
			0, 1, 2,
			1, 3, 2
		};

		plane_key = renderer.push_vertex_positions(std::span(plane_pos));
		renderer.push_vertex_uvs(plane_key, std::span(plane_uv));
		renderer.push_indices16(plane_key, std::span(inds));
	}
	app_timer.print("Loaded plane");
	app_timer.start();

	//Load something from a glTF
	{
		using namespace fastgltf;

		std::filesystem::path glb_path = "models/BoomBox.glb";
		Parser parser;
		GltfDataBuffer data;
		data.loadFromFile(glb_path);
		Expected<Asset> asset = parser.loadBinaryGLTF(&data, glb_path.parent_path());

		printf("Printing node names in \"%s\" ...\n", glb_path.string().c_str());
		for (fastgltf::Node& node : asset->nodes) {
			printf("\t%s\n", node.name.c_str());

			if (node.meshIndex.has_value()) {
				size_t mesh_idx = node.meshIndex.value();
				fastgltf::Mesh& mesh = asset->meshes[mesh_idx];

			}
		}
	}
	app_timer.print("Loaded glTF");
	app_timer.start();

	init_timer.print("App init");

	//Freecam input variables
	bool move_back = false;
	bool move_forward = false;
	bool move_left = false;
	bool move_right = false;
	bool move_down = false;
	bool move_up = false;
	
	//Main loop
	bool running = true;
	uint64_t current_frame = 0;
	double last_frame_took = 0.0001;
	while (running) {
		Timer frame_timer;
		frame_timer.start();
		float delta_time = (float)(last_frame_took / 1000.0);
		
		//These variables are the paradoxically named "InputOutput" variables
		//like the output of the input system, you see
		float mouse_motion_x = 0.0;
		float mouse_motion_y = 0.0;
		{
    		ImGuiIO& io = ImGui::GetIO();
			io.DeltaTime = delta_time;

			//Do input polling loop
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
						io.DisplaySize = ImVec2((float)window.x_resolution, (float)window.y_resolution);
						renderer.frame_uniforms.clip_from_screen = float4x4(
							2.0f / (float)window.x_resolution, 0.0f, 0.0f, -1.0f,
							0.0f, 2.0f / (float)window.y_resolution, 0.0f, -1.0f,
							0.0f, 0.0f, 1.0f, 0.0f,
							0.0f, 0.0f, 0.0f, 1.0f
						);
						break;
					}
					break;
				case SDL_KEYDOWN:
					switch (event.key.keysym.sym) {
					case SDLK_w:
						move_forward = true;
						break;
					case SDLK_s:
						move_back = true;
						break;
					case SDLK_a:
						move_left = true;
						break;
					case SDLK_d:
						move_right = true;
						break;
					case SDLK_q:
						move_down = true;
						break;
					case SDLK_e:
						move_up = true;
						break;
					}

					//Pass keystroke to imgui
					io.AddKeyEvent(SDL2ToImGuiKey(event.key.keysym.sym), true);
					io.AddInputCharacter(event.key.keysym.sym);
					break;
				case SDL_KEYUP:
					switch (event.key.keysym.sym) {
					case SDLK_w:
						move_forward = false;
						break;
					case SDLK_s:
						move_back = false;
						break;
					case SDLK_a:
						move_left = false;
						break;
					case SDLK_d:
						move_right = false;
						break;
					case SDLK_q:
						move_down = false;
						break;
					case SDLK_e:
						move_up = false;
						break;
					}
					io.AddKeyEvent(SDL2ToImGuiKey(event.key.keysym.sym), false);
					break;
				case SDL_MOUSEMOTION:
					mouse_motion_x += (float)event.motion.xrel;
					mouse_motion_y += (float)event.motion.yrel;
					if (!camera_control)
						io.AddMousePosEvent((float)event.motion.x, (float)event.motion.y);
					break;
				case SDL_MOUSEBUTTONDOWN:
					io.AddMouseButtonEvent(SDL2ToImGuiMouseButton(event.button.button), true);
					break;
				case SDL_MOUSEBUTTONUP:
					io.AddMouseButtonEvent(SDL2ToImGuiMouseButton(event.button.button), false);
					
					if (!io.WantCaptureMouse && event.button.button == SDL_BUTTON_RIGHT) {
						camera_control = !camera_control;
						SDL_SetRelativeMouseMode((SDL_bool)camera_control);
						if (camera_control) {
							mouse_saved_x = event.button.x;
							mouse_saved_y = event.button.y;
						} else {
							SDL_WarpMouseInWindow(sdl_window, mouse_saved_x, mouse_saved_y);
						}
					}
					break;
				case SDL_MOUSEWHEEL:
					io.AddMouseWheelEvent(event.wheel.preciseX, event.wheel.preciseY);
					break;
				}
			}

			//We only need to wait for io updates to finish before beginning a new imgui frame
			ImGui::NewFrame();
		}
		
		float time = (float)app_timer.check() * 1.5f / 1000.0f;

		//Move camera
		{
			Camera* main_cam = renderer.cameras.get(main_viewport_camera);
			if (camera_control) {
				const float SENSITIVITY = 0.001;
				main_cam->yaw += mouse_motion_x * SENSITIVITY;
				main_cam->pitch += mouse_motion_y * SENSITIVITY;

				while (main_cam->yaw < -2.0 * M_PI) main_cam->yaw += 2.0 * M_PI;
				while (main_cam->yaw > 2.0 * M_PI) main_cam->yaw -= 2.0 * M_PI;
				if (main_cam->pitch < -M_PI / 2.0) main_cam->pitch = -M_PI / 2.0;
				if (main_cam->pitch > M_PI / 2.0) main_cam->pitch = M_PI / 2.0;
			}

			float4x4 view_matrix = main_cam->make_view_matrix();

			//Do updates that require knowing the view matrix

			const float FREECAM_SPEED = 100.0;
			float4 move_direction = float4(0);
			if (move_forward) move_direction += float4(0.0, 1.0, 0.0, 0.0);
			if (move_back) move_direction += float4(0.0, -1.0, 0.0, 0.0);
			if (move_left) move_direction += float4(-1.0, 0.0, 0.0, 0.0);
			if (move_right) move_direction += float4(1.0, 0.0, 0.0, 0.0);
			if (move_down) move_direction += float4(0.0, 0.0, -1.0, 0.0);
			if (move_up) move_direction += float4(0.0, 0.0, 1.0, 0.0);
			if (length(move_direction) >= float1(0.001)) {
				float4 d = mul(0.1 * normalize(move_direction), view_matrix);
				main_cam->position += FREECAM_SPEED * delta_time * float3(d.x, d.y, d.z);
			}
		}

		//Dear ImGUI update part
		{
			ImGui::ShowDemoWindow();
		}

		//Update per-frame uniforms
		//TODO: This is currently doing nothing to account for multiple in-flight frames
		{
			VulkanBuffer* uniform_buffer = vgd.get_buffer(renderer.frame_uniforms_buffer);
			memcpy(uniform_buffer->alloc_info.pMappedData, &renderer.frame_uniforms, sizeof(FrameUniforms));
		}

		//Update GPU camera data
		std::vector<uint32_t> cam_idx_map;
		cam_idx_map.reserve(renderer.cameras.count());
		{
			std::vector<GPUCamera> g_cameras;
			g_cameras.reserve(renderer.cameras.count());

			for (auto it = renderer.cameras.begin(); it != renderer.cameras.end(); ++it) {
				Camera camera = *it;

				GPUCamera gcam;
				gcam.view_matrix = camera.make_view_matrix();

			    //Transformation applied after view transform to correct axes to match Vulkan clip-space
				//(x-right, y-forward, z-up) -> (x-right, y-down, z-forward)
				float4x4 c_matrix(
					1.0, 0.0, 0.0, 0.0,
					0.0, 0.0, -1.0, 0.0,
					0.0, 1.0, 0.0, 0.0,
					0.0, 0.0, 0.0, 1.0
				);

				float aspect = (float)window.x_resolution / (float)window.y_resolution;
				float desired_fov = M_PI / 2.0f;
				float nearplane = 0.1;
				float farplane = 1000.0;
				float tan_fovy = tanf(desired_fov / 2.0f);
				gcam.projection_matrix = float4x4(
					1.0 / (tan_fovy * aspect), 0.0, 0.0, 0.0,
					0.0, 1.0 / tan_fovy, 0.0, 0.0,
					0.0, 0.0, nearplane / (nearplane - farplane), (nearplane * farplane) / (farplane - nearplane),
					0.0, 0.0, 1.0, 0.0
				);
				gcam.projection_matrix = mul(gcam.projection_matrix, c_matrix);

				g_cameras.push_back(gcam);
				cam_idx_map.push_back(it.underlying_index());
			}

			//Write camera data to GPU buffer in one contiguous push
			//TODO: This is currently doing nothing to account for multiple in-flight frames
			VulkanBuffer* camera_buffer = vgd.get_buffer(renderer.camera_buffer);
			memcpy(camera_buffer->alloc_info.pMappedData, g_cameras.data(), g_cameras.size() * sizeof(GPUCamera));
		}

		//Draw
		{
			//Acquire swapchain image for this frame
			//We want to do this as soon as possible
			uint32_t acquired_image_idx;
			vkAcquireNextImageKHR(vgd.device, window.swapchain, U64_MAX, window.acquire_semaphore, VK_NULL_HANDLE, &acquired_image_idx);

			VkCommandBuffer frame_cb = vgd.command_buffers[current_frame % FRAMES_IN_FLIGHT];
			
			//Wait for command buffer to finish execution before trying to record to it
			if (current_frame >= FRAMES_IN_FLIGHT) {
				uint64_t wait_value = current_frame - FRAMES_IN_FLIGHT + 1;

				VkSemaphoreWaitInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
				info.semaphoreCount = 1;
				info.pSemaphores = &renderer.graphics_timeline_semaphore;
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
			vgd.tick_image_uploads(frame_cb, renderer.descriptor_set);
			uint64_t upload_batches_completed = vgd.get_completed_image_uploads();

			//Check for plane image
			{
				static bool iheartstaticbools = false;
				if (!iheartstaticbools && plane_image_batch_id <= upload_batches_completed) {
					iheartstaticbools = true;
					for (auto it = vgd.available_images.begin(); it != vgd.available_images.end(); ++it) {
						VulkanAvailableImage& image = *it;
						if (plane_image_batch_id == image.batch_id) {
							printf("Found plane image at index %i\n", it.underlying_index());
							plane_image_idx = it.underlying_index();
						}
					}
				}
			}

			vkCmdBindDescriptorSets(frame_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, *vgd.get_pipeline_layout(renderer.pipeline_layout_id), 0, 1, &renderer.descriptor_set, 0, nullptr);

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
				clear_color.color.float32[0] = 0.0f;
				clear_color.color.float32[1] = 0.0f;
				clear_color.color.float32[2] = 0.0f;
				clear_color.color.float32[3] = 1.0f;
				clear_color.depthStencil.depth = 0.0f;
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

			//Bind global index buffer
			vkCmdBindIndexBuffer(frame_cb, vgd.get_buffer(renderer.index_buffer)->buffer, 0, VK_INDEX_TYPE_UINT16);
			
			//Bind pipeline for this pass
			vkCmdBindPipeline(frame_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vgd.get_graphics_pipeline(renderer.ps1_pipeline)->pipeline);

			//Draw hardcoded plane
			{
				//Find GPU index of main viewport camera
				uint32_t main_cam_idx;
				{
					for (uint32_t i = 0; i < cam_idx_map.size(); ++i) {
						if (cam_idx_map[i] == EXTRACT_IDX(main_viewport_camera)) {
							main_cam_idx = i;
							break;
						}
					}
				}

				uint32_t image_idx = imgui_renderer.get_atlas_idx();
				if (plane_image_idx != 0xFFFFFFFF)
					image_idx = plane_image_idx;

				BufferView* plane_positions = renderer.get_vertex_positions(plane_key);
				BufferView* plane_uvs = renderer.get_vertex_uvs(plane_key);
				BufferView* plane_indices = renderer.get_indices16(plane_key);

				uint32_t pcs[] = {
					plane_positions->start / 4,
					plane_uvs->start / 2,
					main_cam_idx,
					image_idx,
					renderer.standard_sampler_idx
				};
				vkCmdPushConstants(frame_cb, *vgd.get_pipeline_layout(renderer.pipeline_layout_id), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 20, pcs);

				vkCmdDrawIndexed(frame_cb, plane_indices->length, 1, plane_indices->start, 0, 0);
			}

			//Record imgui drawing commands into this frame's command buffer
			imgui_renderer.draw(frame_cb, current_frame);

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
				VkSemaphore signal_semaphores[] = { renderer.graphics_timeline_semaphore, window.present_semaphore };
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

		static bool donethis = false;
		if (!donethis) {
			tinyfd_messageBox("Ding!", "We just submitted the first buffer of work to the graphics queue! Wow!", "ok", "info", 1);
			donethis = true;
		}

		//End-of-frame bookkeeping
		current_frame++;
		last_frame_took = frame_timer.check();
	}

	//Wait until all GPU queues have drained before cleaning up resources
	vkDeviceWaitIdle(vgd.device);

	//Cleanup resources
    ImGui::DestroyContext();
	SDL_Quit();

	return 0;
}
