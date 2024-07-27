﻿#ifdef _WIN32
	#define VK_USE_PLATFORM_WIN32_KHR
	#define NOMINMAX
#endif
#ifdef __linux__
	#define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <bit>
#include <chrono>
#include <filesystem>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <thread>
#include <math.h>
#include <hlsl++.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include "volk.h"
#include "stb_image.h"
#include "VulkanWindow.h"
#include "VulkanRenderer.h"
#include "ImguiRenderer.h"
#include "gltf_loader.h"
#include "vma.h"
#include "timer.h"
#include "utils.h"
#include <algorithm>

struct Configuration {
	uint32_t window_width;
	uint32_t window_height;
	uint32_t internal_width;
	uint32_t internal_height;
};

struct DrawPrimitive {
	Key<BufferView> mesh;
	Key<Material> material;
};

struct Ps1Object {
	std::vector<DrawPrimitive> primitives;
	std::vector<uint16_t> primitive_parents;
};

int main(int argc, char* argv[]) {
	PRORENDER_UNUSED_PARAMETER(argc);
	PRORENDER_UNUSED_PARAMETER(argv);

	Timer init_timer = Timer("Init");
	Timer app_timer = Timer("Main function");

	//User config structure

	Configuration my_config = {
		.window_width = 1440,
		.window_height = 1080,
		.internal_width = 640,
		.internal_height = 480
	};

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);	//Initialize SDL
	app_timer.print("SDL Initialization");
	app_timer.start();

	//Init vulkan graphics device
	VulkanGraphicsDevice vgd = VulkanGraphicsDevice();
	app_timer.print("VGD Initialization");
	app_timer.start();

	uint32_t sdl_window_flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
	SDL_Window* sdl_window = SDL_CreateWindow(
		"losing my mind",
		my_config.window_width,
		my_config.window_height,
		sdl_window_flags
	);
	
	//Init the vulkan window
	VkSurfaceKHR window_surface;
	PRORENDER_ASSERT(SDL_Vulkan_CreateSurface(sdl_window, vgd.instance, vgd.alloc_callbacks, &window_surface), SDL_TRUE);
	VulkanWindow window(vgd, window_surface);
	app_timer.print("Window creation");
	app_timer.start();

	//Initialize the renderer
	VulkanRenderer renderer(&vgd, my_config.internal_width, my_config.internal_height);

	//Initialize Dear ImGui
	ImguiRenderer imgui_renderer = ImguiRenderer(
		&vgd,
		ImVec2((float)window.x_resolution, (float)window.y_resolution),
		window.swapchain_renderpass
	);
	app_timer.print("Dear ImGUI Initialization");
	app_timer.start();

	//Load simple 3D plane
	uint64_t miyamoto_image_batch_id;
	uint64_t bird_image_batch_id;
	uint32_t plane_image_count;
	Key<BufferView> plane_mesh_key;
	Key<Material> miyamoto_material_key;
	Key<Material> bird_material_key;
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
			plane_image_count = static_cast<uint32_t>(names.size());
			miyamoto_image_batch_id = vgd.load_image_files(names, formats);
			bird_image_batch_id = vgd.load_image_files(names2, formats);
			hlslpp::float4 base_color(1.0, 1.0, 1.0, 1.0);
			miyamoto_material_key = renderer.push_material(miyamoto_image_batch_id, ImmutableSamplers::STANDARD, base_color);
			bird_material_key = renderer.push_material(bird_image_batch_id, ImmutableSamplers::STANDARD, base_color);
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
	std::filesystem::path glb_paths[] ={
		"models/WaterBottle.glb",
		"models/Lantern.glb",
		"models/Box.glb",
		"models/totoro_backup.glb",
		"models/BoomBox.glb",
		"models/spyro2.glb",
		"models/samus.glb",
		"models/CesiumMan.glb"
	};
	std::vector<Ps1Object> ps1_objects;
	for (auto& path : glb_paths) {
		Key<BufferView> mesh;
		Key<Material> material;

		GLBData ps1_glb = load_glb(path);
		Ps1Object obj = {};
		obj.primitive_parents = ps1_glb.prim_parents;
		printf("GLB has %i primitives\n", (int)ps1_glb.primitives.size());

		std::unordered_map<uint32_t, Key<Material>> seen_materials;
		seen_materials.reserve(ps1_glb.materials.size());

		for (GLBPrimitive& prim : ps1_glb.primitives) {
			//Handle material for this primitive
			if (prim.material_idx != std::numeric_limits<uint32_t>::max()) {
				//If prim has material
				if (seen_materials.contains(prim.material_idx)) {
					material = seen_materials[prim.material_idx];
				} else {
					//If material hasn't been seen before, handle it
					GLBMaterial& mat = ps1_glb.materials[prim.material_idx];
					if (mat.color_image_bytes.size() > 0) {

						CompressedImage image = { .bytes = mat.color_image_bytes};
						uint64_t batch_id = vgd.load_compressed_images({image}, {VK_FORMAT_R8G8B8A8_SRGB});

						material = renderer.push_material(batch_id, ImmutableSamplers::STANDARD, mat.base_color);
					} else {
						material = renderer.push_material(ImmutableSamplers::STANDARD, mat.base_color);
					}
					seen_materials[prim.material_idx] = material;
				}
			} else {
				//If prim has no material, give default one
				material = renderer.push_material(ImmutableSamplers::STANDARD, hlslpp::float4(1.0, 1.0, 1.0, 1.0));
			}

			//Push vertex data into renderer
			mesh = renderer.push_vertex_positions(std::span(prim.positions));
			if (prim.uvs.size() > 0) renderer.push_vertex_uvs(mesh, std::span(prim.uvs));
			if (prim.colors.size() > 0) renderer.push_vertex_colors(mesh, std::span(prim.colors));
			renderer.push_indices16(mesh, std::span(prim.indices));
			
			DrawPrimitive p = {
				.mesh = mesh,
				.material = material
			};
			obj.primitives.push_back(p);
		}
		ps1_objects.push_back(obj);
	}
	app_timer.print("Loaded glbs");
	app_timer.start();

    //Create main camera
	Key<Camera> main_viewport_camera = renderer.cameras.insert(
		{ 
			.position = { -6.92f, -12.922f, 33.767f },
			.yaw = 0.756f,
			.pitch = 0.549f,
			.roll = 0.0f
		}
	);
	bool camera_control = false;
	float mouse_saved_x = 0.0, mouse_saved_y = 0.0;

	//Freecam input variables
	bool move_back = false;
	bool move_forward = false;
	bool move_left = false;
	bool move_right = false;
	bool move_down = false;
	bool move_up = false;
	bool camera_rolling = false;
	bool camera_boost = false;

	bool do_imgui = true;
	bool frame_advance = false;
	float timescale = 0.0;

	init_timer.print("App init");
	
	//Main loop
	bool running = true;
	uint64_t current_tick = 0;
	double last_frame_took = 0.0001;
	while (running) {
		static uint64_t current_frame = 0;
		Timer frame_timer;
		frame_timer.start();
		float delta_time = (float)(last_frame_took / 1000.0);
		if (delta_time > 0.20f) delta_time = 0.20f;
		
		//The distance the mouse has moved on each axis
		float mouse_motion_x = 0.0;
		float mouse_motion_y = 0.0;
		static bool show_demo = true;
		bool do_update_and_render = !frame_advance;
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
					case SDLK_LSHIFT:
						camera_boost = true;
						break;
					case SDLK_SPACE:
						frame_advance = !frame_advance;
						break;
					case SDLK_BACKSLASH:
						do_update_and_render = true;
						frame_advance = true;
						break;
					case SDLK_ESCAPE:
						do_imgui = !do_imgui;
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
					case SDLK_LSHIFT:
						camera_boost = false;
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
		}
		
		if (do_update_and_render) {
			//float simulation_time = timescale * (float)app_timer.check() / 1000.0f;
			ImGui::NewFrame();

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

					while (main_cam->yaw < -2.0f * PI) main_cam->yaw += (float)(2.0 * PI);
					while (main_cam->yaw > 2.0f * PI) main_cam->yaw -= (float)(2.0 * PI);
					while (main_cam->roll < -2.0f * PI) main_cam->roll += (float)(2.0 * PI);
					while (main_cam->roll > 2.0f * PI) main_cam->roll -= (float)(2.0 * PI);
					if (main_cam->pitch < -PI / 2.0f) main_cam->pitch = (float)(-PI / 2.0);
					if (main_cam->pitch > PI / 2.0f) main_cam->pitch = (float)(PI / 2.0);
				}

				float4x4 view_matrix = main_cam->make_view_matrix();

				//Do updates that require knowing the view matrix

				float camera_speed = 100.0;
				if (camera_boost) {
					camera_speed *= 10.0;
				}
				float4 move_direction = float4(0);
				if (move_forward) move_direction += float4(0.0, 1.0, 0.0, 0.0);
				if (move_back) move_direction += float4(0.0, -1.0, 0.0, 0.0);
				if (move_left) move_direction += float4(-1.0, 0.0, 0.0, 0.0);
				if (move_right) move_direction += float4(1.0, 0.0, 0.0, 0.0);
				if (move_down) move_direction += float4(0.0, 0.0, -1.0, 0.0);
				if (move_up) move_direction += float4(0.0, 0.0, 1.0, 0.0);
				if (length(move_direction) >= float1(0.001)) {
					float4 d = mul(0.1 * normalize(move_direction), view_matrix);
					main_cam->position += camera_speed * delta_time * float3(d.x, d.y, d.z);
				}
			}

			//Dear ImGUI update part
			if (do_imgui) {
				if (show_demo)
					ImGui::ShowDemoWindow(&show_demo);

				{
					ImGui::Text("Frame number: %i", current_frame);
					Camera* camera = renderer.cameras.get(main_viewport_camera);
					ImGui::Text("Camera position: (%f, %f, %f)", camera->position[0], camera->position[1], camera->position[2]);
					ImGui::Text("Camera pitch: %f", camera->pitch);
					ImGui::Text("Camera yaw: %f", camera->yaw);
					ImGui::Text("Camera roll: %f", camera->roll);
					ImGui::SliderFloat("Timescale", &timescale, 0.0, 2.0);
				}

				ImGuiWindowFlags window_flags = 0;
				ImGui::Begin("Texture inspector", nullptr, window_flags);
				
				static int dimension_max = 300;
				ImGui::SliderInt("Display width", &dimension_max, 1, 1500);
				ImGui::Separator();

				for (auto it = vgd.bindless_images.begin(); it != vgd.bindless_images.end(); ++it) {
					VulkanBindlessImage& image = *it;

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

			//Draw floor plane
			{
				hlslpp::float4x4 mat(
					20.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 20.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 20.0f, 0.0f,
					0.0f, 0.0f, 0.0f, 1.0f
				);
				InstanceData mats[] = {mat};
				renderer.ps1_draw(plane_mesh_key, miyamoto_material_key, std::span(mats));
			}

			for (uint32_t i = 0; i < ps1_objects.size(); i++) {
				Ps1Object& p = ps1_objects[i];
				
				hlslpp::float3 scale = {1.0, 1.0, 1.0};
				hlslpp::float4x4 scale_mat(
					scale[0], 0.0f, 0.0f, 1.0f,
					0.0f, scale[1], 0.0f, 1.0f,
					0.0f, 0.0f, scale[2], 1.0f,
					0.0f, 0.0f, 0.0f, 1.0f
				);
				
				hlslpp::float4x4 mat(
					10.0, 0.0, 0.0, (50.0f * i),
					0.0, 10.0, 0.0, 0.0,
					0.0, 0.0, 10.0, 0.0,
					0.0, 0.0, 0.0, 1.0	
				);

				static float rotation = 0.0f;
				float cosyaw = cosf(-1.5f * rotation);
				float sinyaw = sinf(-1.5f * rotation);
				hlslpp::float4x4 yaw_matrix(
					cosyaw, -sinyaw, 0.0, 0.0,
					sinyaw, cosyaw, 0.0, 0.0,
					0.0, 0.0, 1.0, 0.0,
					0.0, 0.0, 0.0, 1.0
				);
				InstanceData mats[] = {hlslpp::mul(yaw_matrix, hlslpp::mul(mat, scale_mat))};
				for (DrawPrimitive& prim : p.primitives) {
					renderer.ps1_draw(prim.mesh, prim.material, std::span(mats));
				}
				rotation += timescale * delta_time;
			}

			//Queue the static plane to be drawn
			Timer plane_update_timer;
			plane_update_timer.start();
			{
				using namespace hlslpp;

				static int number = 1000;

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
						0.0, 0.0, 1.0, (float)i * 3.0f + 20.0f,
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
						0.0, 0.0, 1.0, (float)i * 3.0f + 20.0f,
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
				while (rotation > 2.0f * PI) rotation -= (float)(2.0 * PI);
			}

			//Draw
			{
				VkCommandBuffer frame_cb = vgd.get_graphics_command_buffer();
				
				renderer.cpu_sync();

				//Per-frame checking of pending images to see if they're ready
				vgd.tick_image_uploads(frame_cb);

				SyncData sync = {};
				SwapchainFramebuffer window_framebuffer = window.acquire_next_image(vgd, sync, current_frame);

				renderer.render(frame_cb, sync);

				vgd.begin_render_pass(frame_cb, window_framebuffer.fb);

				imgui_renderer.draw(frame_cb, current_frame);
				vgd.end_render_pass(frame_cb);
				
				vgd.graphics_queue_submit(frame_cb, sync);
				vgd.return_command_buffer(frame_cb, current_frame + 1, renderer.frames_completed_semaphore);
				window.present_framebuffer(vgd, window_framebuffer, sync);
				current_frame += 1;
			}
			
			//End-of-frame bookkeeping
			current_tick++;
			last_frame_took = frame_timer.check();
		}

	}

	//Wait until all GPU queues have drained before cleaning up resources
	vkDeviceWaitIdle(vgd.device);

	//Cleanup resources
    ImGui::DestroyContext();
	SDL_Quit();

	return 0;
}
