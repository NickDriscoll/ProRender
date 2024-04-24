#include "ProRender.h"
#include "ImguiRenderer.h"
#include "tinyfiledialogs.h"
#include <algorithm>

#define _USE_MATH_DEFINES
#include <math.h>

int main(int argc, char* argv[]) {
	Timer init_timer = Timer("Init");
	Timer app_timer = Timer("Main function");

	//User config structure

	Configuration my_config = {
		.window_width = 1200,
		.window_height = 800	
	};

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);	//Initialize SDL
	app_timer.print("SDL Initialization");
	app_timer.start();

	//Init vulkan graphics device
	VulkanGraphicsDevice vgd = VulkanGraphicsDevice();
	app_timer.print("VGD Initialization");
	app_timer.start();

	uint32_t window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
	SDL_Window* sdl_window = SDL_CreateWindow(
		"losing my mind",
		my_config.window_width,
		my_config.window_height,
		window_flags
	);
	
	//Init the vulkan window
	VkSurfaceKHR window_surface;
	if (SDL_Vulkan_CreateSurface(sdl_window, vgd.instance, vgd.alloc_callbacks, &window_surface) == SDL_FALSE) {
		printf("Creating VkSurface failed.\n");
		exit(-1);
	}
	VulkanWindow window(vgd, window_surface);
	app_timer.print("Window creation");
	app_timer.start();

	//Initialize the renderer
	VulkanRenderer renderer(&vgd, window.swapchain_renderpass);
    renderer.frame_uniforms.clip_from_screen = hlslpp::float4x4(
        2.0f / (float)window.x_resolution, 0.0f, 0.0f, -1.0f,
        0.0f, 2.0f / (float)window.y_resolution, 0.0f, -1.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

	//Initialize Dear ImGui
	ImguiRenderer imgui_renderer = ImguiRenderer(
		&vgd,
		renderer.point_sampler_idx,
		ImVec2((float)window.x_resolution, (float)window.y_resolution),
		renderer.pipeline_layout_id,
		window.swapchain_renderpass,
		renderer.descriptor_set
	);
	app_timer.print("Dear ImGUI Initialization");
	app_timer.start();

    //Create main camera
	Key<Camera> main_viewport_camera = renderer.cameras.insert({ .position = { 1.0f, -2.0f, 5.0f }, .pitch = 1.3f });
	bool camera_control = false;
	float mouse_saved_x, mouse_saved_y;

	//Load simple 3D plane
	uint64_t miyamoto_image_batch_id;
	uint64_t bird_image_batch_id;
	uint32_t plane_image_count;
	Key<BufferView> plane_mesh_key;
	Key<Material> miyamoto_material_key;
	Key<Material> bird_material_key;
	uint32_t plane_image_idx = 0xFFFFFFFF;
	bool know_plane_image = false;
	{
		//Load plane texture
		{
			std::vector<const char*> names = {
				"images/stressed-miyamoto.jpg",
			};
			std::vector<const char*> names2 = {
				"images/birds-allowed.png"
			};
			std::vector<VkFormat> formats = {
				VK_FORMAT_R8G8B8A8_SRGB
			};
			std::vector<VkFormat> formats2 = {
				VK_FORMAT_R8G8B8A8_SRGB
			};
			plane_image_count = names.size();
			miyamoto_image_batch_id = vgd.load_image_files(names, formats);
			bird_image_batch_id = vgd.load_image_files(names2, formats2);
			hlslpp::float4 base_color(1.0, 1.0, 1.0, 1.0);
			miyamoto_material_key = renderer.push_material(miyamoto_image_batch_id, renderer.standard_sampler_idx, base_color);
			bird_material_key = renderer.push_material(bird_image_batch_id, renderer.standard_sampler_idx, base_color);
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

		plane_mesh_key = renderer.push_vertex_positions(std::span(plane_pos));
		renderer.push_vertex_uvs(plane_mesh_key, std::span(plane_uv));
		renderer.push_indices16(plane_mesh_key, std::span(inds));
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
		Expected<Asset> asset = parser.loadGltfBinary(&data, glb_path.parent_path());

		printf("Printing node names in \"%s\" ...\n", glb_path.string().c_str());
		for (Node& node : asset->nodes) {
			printf("\t%s\n", node.name.c_str());

			if (node.meshIndex.has_value()) {
				size_t mesh_idx = node.meshIndex.value();
				Mesh& mesh = asset->meshes[mesh_idx];

			}
		}
	}
	app_timer.print("Loaded glTF");
	app_timer.start();

	//Freecam input variables
	bool move_back = false;
	bool move_forward = false;
	bool move_left = false;
	bool move_right = false;
	bool move_down = false;
	bool move_up = false;
	bool camera_rolling = false;

	uint32_t which_image = 0;

	init_timer.print("App init");
	
	//Main loop
	bool running = true;
	uint64_t current_tick = 0;
	double last_frame_took = 0.0001;
	while (running) {
		Timer frame_timer;
		frame_timer.start();
		float delta_time = (float)(last_frame_took / 1000.0);
		
		//The distance the mouse has moved on each axis
		float mouse_motion_x = 0.0;
		float mouse_motion_y = 0.0;
		{
    		ImGuiIO& io = ImGui::GetIO();
			io.DeltaTime = delta_time;

			//Do input polling loop
			SDL_Event event;
			while (SDL_PollEvent(&event)) {
				switch (event.type) {
				case SDL_EVENT_QUIT:
					running = false;
					break;
				case SDL_EVENT_WINDOW_RESIZED:
					window.resize(vgd);
					io.DisplaySize = ImVec2((float)window.x_resolution, (float)window.y_resolution);
					renderer.frame_uniforms.clip_from_screen = hlslpp::float4x4(
						2.0f / (float)window.x_resolution, 0.0f, 0.0f, -1.0f,
						0.0f, 2.0f / (float)window.y_resolution, 0.0f, -1.0f,
						0.0f, 0.0f, 1.0f, 0.0f,
						0.0f, 0.0f, 0.0f, 1.0f
					);
					break;
				case SDL_EVENT_KEY_DOWN:
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
					case SDLK_LALT:
						camera_rolling = true;
						break;
					case SDLK_SPACE:
						which_image = (which_image + 1) % plane_image_count;
						know_plane_image = false;
						break;
					}

					//Pass keystroke to imgui
					io.AddKeyEvent(SDL2ToImGuiKey(event.key.keysym.sym), true);
					io.AddInputCharacter(event.key.keysym.sym);
					break;
				case SDL_EVENT_KEY_UP:
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
					case SDLK_LALT:
						camera_rolling = false;
						break;
					}
					io.AddKeyEvent(SDL2ToImGuiKey(event.key.keysym.sym), false);
					break;
				case SDL_EVENT_MOUSE_MOTION:
					mouse_motion_x += (float)event.motion.xrel;
					mouse_motion_y += (float)event.motion.yrel;
					if (!camera_control)
						io.AddMousePosEvent((float)event.motion.x, (float)event.motion.y);
					break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
					io.AddMouseButtonEvent(SDL2ToImGuiMouseButton(event.button.button), true);
					break;
				case SDL_EVENT_MOUSE_BUTTON_UP:
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
				case SDL_EVENT_MOUSE_WHEEL:
					io.AddMouseWheelEvent(event.wheel.x, event.wheel.y);
					break;
				}
			}

			//We only need to wait for io updates to finish before beginning a new imgui frame
			ImGui::NewFrame();
		}
		
		float time = (float)app_timer.check() * 1.5f / 1000.0f;

		//Move camera
		{
			using namespace hlslpp;

			Camera* main_cam = renderer.cameras.get(main_viewport_camera);
			if (camera_control) {
				const float SENSITIVITY = 0.001f;
				if (camera_rolling) {
					main_cam->roll += mouse_motion_x * SENSITIVITY;
				} else {
					main_cam->yaw += mouse_motion_x * SENSITIVITY;
					main_cam->pitch += mouse_motion_y * SENSITIVITY;
				}

				while (main_cam->yaw < -2.0f * M_PI) main_cam->yaw += (float)(2.0 * M_PI);
				while (main_cam->yaw > 2.0f * M_PI) main_cam->yaw -= (float)(2.0 * M_PI);
				while (main_cam->roll < -2.0f * M_PI) main_cam->roll += (float)(2.0 * M_PI);
				while (main_cam->roll > 2.0f * M_PI) main_cam->roll -= (float)(2.0 * M_PI);
				if (main_cam->pitch < -M_PI / 2.0f) main_cam->pitch = (float)(-M_PI / 2.0);
				if (main_cam->pitch > M_PI / 2.0f) main_cam->pitch = (float)(M_PI / 2.0);
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
			static bool show_demo = true;
			if (show_demo)
				ImGui::ShowDemoWindow(&show_demo);

			ImGuiWindowFlags window_flags = 0;
			ImGui::Begin("Texture inspector", nullptr, window_flags);
			
			static int dimension_max = 300;
			ImGui::SliderInt("Display width", &dimension_max, 1, 1500);
			ImGui::Separator();

			for (auto it = vgd.available_images.begin(); it != vgd.available_images.end(); ++it) {
				VulkanAvailableImage& image = *it;

				ImGui::Text("From batch #%i", image.batch_id);

				ImVec2 dims = ImVec2((float)image.vk_image.width, (float)image.vk_image.height);
				ImGui::BulletText("Width = %i", (uint32_t)dims.x);
				ImGui::BulletText("Height = %i", (uint32_t)dims.y);

				float aspect_ratio = dims.x / dims.y;
				dims.x = std::min(dims.x, (float)dimension_max);
				dims.y = dims.x / aspect_ratio;
				
            	ImVec2 pos = ImGui::GetCursorScreenPos();
				ImVec2 uv_min = ImVec2(0.0, 0.0);
				ImVec2 uv_max = ImVec2(1.0, 1.0);
				ImVec4 tint_col = ImGui::GetStyleColorVec4(ImGuiCol_Text);
				ImVec4 border_col = ImGui::GetStyleColorVec4(ImGuiCol_Border);
				ImGui::Image((ImTextureID)(uint64_t)it.slot_index(), dims, uv_min, uv_max, tint_col, border_col);

				if (ImGui::BeginItemTooltip()) {
					ImGuiIO& io = ImGui::GetIO();

					float region_sz = 32.0f;
					float region_x = io.MousePos.x - pos.x - region_sz * 0.5f;
					float region_y = io.MousePos.y - pos.y - region_sz * 0.5f;
					float zoom = 4.0f;
					if (region_x < 0.0f) { region_x = 0.0f; }
					else if (region_x > dims.x - region_sz) { region_x = dims.x - region_sz; }
					if (region_y < 0.0f) { region_y = 0.0f; }
					else if (region_y > dims.y - region_sz) { region_y = dims.y - region_sz; }
					ImGui::Text("Min: (%.2f, %.2f)", region_x, region_y);
					ImGui::Text("Max: (%.2f, %.2f)", region_x + region_sz, region_y + region_sz);
					ImVec2 uv0 = ImVec2((region_x) / dims.x, (region_y) / dims.y);
					ImVec2 uv1 = ImVec2((region_x + region_sz) / dims.x, (region_y + region_sz) / dims.y);
					ImGui::Image((ImTextureID)(uint64_t)it.slot_index(), ImVec2(region_sz * zoom, region_sz * zoom), uv0, uv1, tint_col, border_col);
					ImGui::EndTooltip();
				}
				ImGui::Separator();
			}
			ImGui::End();
		}

		//Process resource deletion queue(s)
		vgd.service_deletion_queues();

		//Queue the static plane to be drawn
		{
			using namespace hlslpp;

			int number = 100;
			std::vector<InstanceData> tforms;
			tforms.reserve(number);
			static float rotation = 0.0f;
			for (int i = 0; i < number; i++) {
				float yaw = (float)i * rotation;
				float cosyaw = cosf(yaw);
				float sinyaw = sinf(yaw);
				float4x4 yaw_matrix(
					cosyaw, -sinyaw, 0.0, 0.0,
					sinyaw, cosyaw, 0.0, 0.0,
					0.0, 0.0, 1.0, 0.0,
					0.0, 0.0, 0.0, 1.0
				);
				float4x4 matrix(
					1.0, 0.0, 0.0, 0.0,
					0.0, 1.0, 0.0, 0.0,
					0.0, 0.0, 1.0, (float)i * 3.0,
					0.0, 0.0, 0.0, 1.0
				);
				tforms.push_back(
					{
						.world_from_model = mul(matrix, yaw_matrix)
					}
				);
			}
			renderer.ps1_draw(plane_mesh_key, miyamoto_material_key, std::span(tforms));
			tforms.clear();

			for (int i = 0; i < number; i++) {
				float yaw = (float)i * rotation;
				float cosyaw = cosf(yaw);
				float sinyaw = sinf(yaw);
				float4x4 yaw_matrix(
					cosyaw, -sinyaw, 0.0, 0.0,
					sinyaw, cosyaw, 0.0, 0.0,
					0.0, 0.0, 1.0, 0.0,
					0.0, 0.0, 0.0, 1.0
				);
				float4x4 matrix(
					1.0, 0.0, 0.0, 30.0,
					0.0, 1.0, 0.0, 0.0,
					0.0, 0.0, 1.0, (float)i * 3.0,
					0.0, 0.0, 0.0, 1.0
				);
				tforms.push_back(
					{
						.world_from_model = mul(matrix, yaw_matrix)
					}
				);
			}
			renderer.ps1_draw(plane_mesh_key, bird_material_key, std::span(tforms));

			rotation += delta_time;
			while (rotation > 2.0f * M_PI) rotation -= (float)(2.0 * M_PI);
		}

		//Draw
		{
			uint64_t current_frame = renderer.get_current_frame();
			VkCommandBuffer frame_cb = vgd.get_graphics_command_buffer();
			
			//Per-frame checking of pending images to see if they're ready
			vgd.tick_image_uploads(frame_cb, renderer.descriptor_set, DescriptorBindings::SAMPLED_IMAGES);
		

			SyncData sync = {};
			SwapchainFramebuffer window_framebuffer = window.acquire_next_image(vgd, sync, current_frame);

			vgd.begin_render_pass(frame_cb, window_framebuffer.fb);
			renderer.render(frame_cb, window_framebuffer.fb, sync);
			imgui_renderer.draw(frame_cb, window_framebuffer.fb, current_frame);
			vgd.end_render_pass(frame_cb);

			vgd.graphics_queue_submit(frame_cb, sync);
			
			vgd.return_command_buffer(frame_cb, current_frame + 1, renderer.frames_completed_semaphore);
			window.present_framebuffer(vgd, window_framebuffer, sync);
		}
		
		//End-of-frame bookkeeping
		current_tick++;
		last_frame_took = frame_timer.check();
	}

	//Wait until all GPU queues have drained before cleaning up resources
	vkDeviceWaitIdle(vgd.device);

	//Cleanup resources
    ImGui::DestroyContext();
	SDL_Quit();

	return 0;
}
