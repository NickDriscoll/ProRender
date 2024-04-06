#include "ImguiRenderer.h"
#include <bit>
#include <SDL3/SDL.h>

ImGuiKey SDL2ToImGuiKey(int keycode) {
    switch (keycode)
    {
        case SDLK_TAB: return ImGuiKey_Tab;
        case SDLK_LEFT: return ImGuiKey_LeftArrow;
        case SDLK_RIGHT: return ImGuiKey_RightArrow;
        case SDLK_UP: return ImGuiKey_UpArrow;
        case SDLK_DOWN: return ImGuiKey_DownArrow;
        case SDLK_PAGEUP: return ImGuiKey_PageUp;
        case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
        case SDLK_HOME: return ImGuiKey_Home;
        case SDLK_END: return ImGuiKey_End;
        case SDLK_INSERT: return ImGuiKey_Insert;
        case SDLK_DELETE: return ImGuiKey_Delete;
        case SDLK_BACKSPACE: return ImGuiKey_Backspace;
        case SDLK_SPACE: return ImGuiKey_Space;
        case SDLK_RETURN: return ImGuiKey_Enter;
        case SDLK_ESCAPE: return ImGuiKey_Escape;
        case SDLK_QUOTE: return ImGuiKey_Apostrophe;
        case SDLK_COMMA: return ImGuiKey_Comma;
        case SDLK_MINUS: return ImGuiKey_Minus;
        case SDLK_PERIOD: return ImGuiKey_Period;
        case SDLK_SLASH: return ImGuiKey_Slash;
        case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
        case SDLK_EQUALS: return ImGuiKey_Equal;
        case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
        case SDLK_BACKSLASH: return ImGuiKey_Backslash;
        case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
        case SDLK_BACKQUOTE: return ImGuiKey_GraveAccent;
        case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
        case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
        case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
        case SDLK_PAUSE: return ImGuiKey_Pause;
        case SDLK_KP_0: return ImGuiKey_Keypad0;
        case SDLK_KP_1: return ImGuiKey_Keypad1;
        case SDLK_KP_2: return ImGuiKey_Keypad2;
        case SDLK_KP_3: return ImGuiKey_Keypad3;
        case SDLK_KP_4: return ImGuiKey_Keypad4;
        case SDLK_KP_5: return ImGuiKey_Keypad5;
        case SDLK_KP_6: return ImGuiKey_Keypad6;
        case SDLK_KP_7: return ImGuiKey_Keypad7;
        case SDLK_KP_8: return ImGuiKey_Keypad8;
        case SDLK_KP_9: return ImGuiKey_Keypad9;
        case SDLK_KP_PERIOD: return ImGuiKey_KeypadDecimal;
        case SDLK_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case SDLK_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case SDLK_KP_MINUS: return ImGuiKey_KeypadSubtract;
        case SDLK_KP_PLUS: return ImGuiKey_KeypadAdd;
        case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
        case SDLK_KP_EQUALS: return ImGuiKey_KeypadEqual;
        case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
        case SDLK_LSHIFT: return ImGuiKey_LeftShift;
        case SDLK_LALT: return ImGuiKey_LeftAlt;
        case SDLK_LGUI: return ImGuiKey_LeftSuper;
        case SDLK_RCTRL: return ImGuiKey_RightCtrl;
        case SDLK_RSHIFT: return ImGuiKey_RightShift;
        case SDLK_RALT: return ImGuiKey_RightAlt;
        case SDLK_RGUI: return ImGuiKey_RightSuper;
        case SDLK_APPLICATION: return ImGuiKey_Menu;
        case SDLK_0: return ImGuiKey_0;
        case SDLK_1: return ImGuiKey_1;
        case SDLK_2: return ImGuiKey_2;
        case SDLK_3: return ImGuiKey_3;
        case SDLK_4: return ImGuiKey_4;
        case SDLK_5: return ImGuiKey_5;
        case SDLK_6: return ImGuiKey_6;
        case SDLK_7: return ImGuiKey_7;
        case SDLK_8: return ImGuiKey_8;
        case SDLK_9: return ImGuiKey_9;
        case SDLK_a: return ImGuiKey_A;
        case SDLK_b: return ImGuiKey_B;
        case SDLK_c: return ImGuiKey_C;
        case SDLK_d: return ImGuiKey_D;
        case SDLK_e: return ImGuiKey_E;
        case SDLK_f: return ImGuiKey_F;
        case SDLK_g: return ImGuiKey_G;
        case SDLK_h: return ImGuiKey_H;
        case SDLK_i: return ImGuiKey_I;
        case SDLK_j: return ImGuiKey_J;
        case SDLK_k: return ImGuiKey_K;
        case SDLK_l: return ImGuiKey_L;
        case SDLK_m: return ImGuiKey_M;
        case SDLK_n: return ImGuiKey_N;
        case SDLK_o: return ImGuiKey_O;
        case SDLK_p: return ImGuiKey_P;
        case SDLK_q: return ImGuiKey_Q;
        case SDLK_r: return ImGuiKey_R;
        case SDLK_s: return ImGuiKey_S;
        case SDLK_t: return ImGuiKey_T;
        case SDLK_u: return ImGuiKey_U;
        case SDLK_v: return ImGuiKey_V;
        case SDLK_w: return ImGuiKey_W;
        case SDLK_x: return ImGuiKey_X;
        case SDLK_y: return ImGuiKey_Y;
        case SDLK_z: return ImGuiKey_Z;
        case SDLK_F1: return ImGuiKey_F1;
        case SDLK_F2: return ImGuiKey_F2;
        case SDLK_F3: return ImGuiKey_F3;
        case SDLK_F4: return ImGuiKey_F4;
        case SDLK_F5: return ImGuiKey_F5;
        case SDLK_F6: return ImGuiKey_F6;
        case SDLK_F7: return ImGuiKey_F7;
        case SDLK_F8: return ImGuiKey_F8;
        case SDLK_F9: return ImGuiKey_F9;
        case SDLK_F10: return ImGuiKey_F10;
        case SDLK_F11: return ImGuiKey_F11;
        case SDLK_F12: return ImGuiKey_F12;
        case SDLK_F13: return ImGuiKey_F13;
        case SDLK_F14: return ImGuiKey_F14;
        case SDLK_F15: return ImGuiKey_F15;
        case SDLK_F16: return ImGuiKey_F16;
        case SDLK_F17: return ImGuiKey_F17;
        case SDLK_F18: return ImGuiKey_F18;
        case SDLK_F19: return ImGuiKey_F19;
        case SDLK_F20: return ImGuiKey_F20;
        case SDLK_F21: return ImGuiKey_F21;
        case SDLK_F22: return ImGuiKey_F22;
        case SDLK_F23: return ImGuiKey_F23;
        case SDLK_F24: return ImGuiKey_F24;
        case SDLK_AC_BACK: return ImGuiKey_AppBack;
        case SDLK_AC_FORWARD: return ImGuiKey_AppForward;
    }
    return ImGuiKey_None;
}

int SDL2ToImGuiMouseButton(int button) {
    int ret = button;
    switch (button) {
    case SDL_BUTTON_MIDDLE:
        ret = SDL_BUTTON_RIGHT;
        break;
    case SDL_BUTTON_RIGHT:
        ret = SDL_BUTTON_MIDDLE;
        break;
    default:
        break;
    }

    return ret - 1;
}



ImguiRenderer::ImguiRenderer(
	VulkanGraphicsDevice* v,
	uint32_t sampler,
	ImVec2 window_size,
	Key<VkPipelineLayout> pipeline_layout_id,
	Key<VkRenderPass> renderpass,
	VkDescriptorSet& descriptor_set
) {
    vgd = v;
    sampler_idx = sampler;

    //Initialize Dear ImGui
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
    	ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = window_size;

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
		uint64_t batch_id = vgd->load_raw_images(images, formats);

		VkSemaphoreWaitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
		info.semaphoreCount = 1;
		info.pSemaphores = &vgd->image_upload_semaphore;
		info.pValues = &batch_id;
		if (vkWaitSemaphores(vgd->device, &info, U64_MAX) != VK_SUCCESS) {
			printf("Waiting for graphics timeline semaphore failed.\n");
			exit(-1);
		}
	
		uint32_t tex_index = 0;
		for (auto it = vgd->available_images.begin(); it != vgd->available_images.end(); ++it) {
			VulkanAvailableImage image = *it;
			if (batch_id == image.batch_id) {
				tex_index = it.slot_index();
				break;
			}
		}

		atlas_idx = tex_index;
		io.Fonts->SetTexID((ImTextureID)(uint64_t)tex_index);
	}

    //Allocate memory for ImGUI vertex data
    //TODO: probabaly shouldn't be in renderer init
    {
        VkDeviceSize buffer_size = 1024 * 1024;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        alloc_info.priority = 1.0;

        position_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
        uv_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
        color_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
        index_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, alloc_info);
    }

	//Update bindless descriptor set with imgui related bindings
	{
		std::vector<VkWriteDescriptorSet> descriptor_writes;

        VkDescriptorBufferInfo im_pos_buffer_info = {
            .buffer = vgd->get_buffer(position_buffer)->buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };
    
        VkWriteDescriptorSet im_pos_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 3,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &im_pos_buffer_info
        };
        descriptor_writes.push_back(im_pos_write);
    
        VkDescriptorBufferInfo im_uv_buffer_info = {
            .buffer = vgd->get_buffer(uv_buffer)->buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };
    
        VkWriteDescriptorSet im_uv_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 4,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &im_uv_buffer_info
        };
        descriptor_writes.push_back(im_uv_write);
    
        VkDescriptorBufferInfo im_col_buffer_info = {
            .buffer = vgd->get_buffer(color_buffer)->buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };
    
        VkWriteDescriptorSet im_col_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 5,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &im_col_buffer_info
        };
        descriptor_writes.push_back(im_col_write);

		vkUpdateDescriptorSets(vgd->device, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);
	}

	//Create graphics pipeline
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
			{
				.cullMode = VK_CULL_MODE_NONE
			},
			{}
		};

		VulkanMultisampleState ms_states[] = {
			{},
			{}
		};

		VulkanDepthStencilState ds_states[] = {
			{
				.depthTestEnable = VK_FALSE
			},
			{
				.depthTestEnable = VK_FALSE
			}
		};

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

		const char* shaders[] = { "shaders/imgui.vert.spv", "shaders/imgui.frag.spv" };

		graphics_pipeline_layout = pipeline_layout_id;
		vgd->create_graphics_pipelines(
			pipeline_layout_id,
			renderpass,
			shaders,
			ia_states,
			tess_states,
			vs_states,
			rast_states,
			ms_states,
			ds_states,
			blend_states,
			&graphics_pipeline,
			1
		);
	}

}

ImguiRenderer::~ImguiRenderer() {
	vgd->destroy_buffer(position_buffer);
	vgd->destroy_buffer(uv_buffer);
	vgd->destroy_buffer(color_buffer);
	vgd->destroy_buffer(index_buffer);
}

void ImguiRenderer::draw(VkCommandBuffer& frame_cb, uint64_t frame_counter) {
	//Upload ImGUI triangle data and record ImGUI draw commands
	
	uint32_t frame_slot = frame_counter % FRAMES_IN_FLIGHT;
	uint32_t last_frame_slot = frame_slot == 0 ? FRAMES_IN_FLIGHT - 1 : frame_slot - 1;
	ImguiFrame& current_frame = frames[frame_slot];
	ImguiFrame& last_frame = frames[last_frame_slot];

	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();

	//TODO: This local offset logic might only hold when FRAMES_IN_FLIGHT == 2
	uint32_t current_vertex_offset = 0;
	if (last_frame.vertex_start < (uint32_t)draw_data->TotalVtxCount) {
		current_vertex_offset = last_frame.vertex_start + last_frame.vertex_size;
	}
	current_frame.vertex_start = current_vertex_offset;
	current_frame.vertex_size = draw_data->TotalVtxCount;

	uint32_t current_index_offset = 0;
	if (last_frame.index_start < (uint32_t)draw_data->TotalIdxCount) {
		current_index_offset = last_frame.index_start + last_frame.index_size;
	}
	current_frame.index_start = current_index_offset;
	current_frame.index_size = draw_data->TotalIdxCount;

	VulkanBuffer* gpu_imgui_positions = vgd->get_buffer(position_buffer);
	VulkanBuffer* gpu_imgui_uvs = vgd->get_buffer(uv_buffer);
	VulkanBuffer* gpu_imgui_colors = vgd->get_buffer(color_buffer);
	VulkanBuffer* gpu_imgui_indices = vgd->get_buffer(index_buffer);
	
	uint8_t* gpu_pos_ptr = std::bit_cast<uint8_t*>(gpu_imgui_positions->alloc_info.pMappedData);
	uint8_t* gpu_uv_ptr = std::bit_cast<uint8_t*>(gpu_imgui_uvs->alloc_info.pMappedData);
	uint8_t* gpu_col_ptr = std::bit_cast<uint8_t*>(gpu_imgui_colors->alloc_info.pMappedData);
	uint8_t* gpu_idx_ptr = std::bit_cast<uint8_t*>(gpu_imgui_indices->alloc_info.pMappedData);

	//Record once-per-frame binding of index buffer and graphics pipeline
	vkCmdBindIndexBuffer(frame_cb, gpu_imgui_indices->buffer, current_index_offset * sizeof(ImDrawIdx), VK_INDEX_TYPE_UINT16);
	vkCmdBindPipeline(frame_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vgd->get_graphics_pipeline(graphics_pipeline)->pipeline);

	uint32_t pcs[] = { atlas_idx, sampler_idx };
	
	//Create intermediate buffers for Imgui vertex attributes
	std::vector<uint8_t> imgui_positions;
	imgui_positions.resize(draw_data->TotalVtxCount * sizeof(float) * 2);
	std::vector<uint8_t> imgui_uvs;
	imgui_uvs.resize(draw_data->TotalVtxCount * sizeof(float) * 2);
	std::vector<uint8_t> imgui_colors;
	imgui_colors.resize(draw_data->TotalVtxCount * sizeof(uint32_t));

	uint32_t vtx_offset = 0;
	uint32_t idx_offset = 0;
	for (int32_t i = 0; i < draw_data->CmdListsCount; i++) {
		ImDrawList* draw_list = draw_data->CmdLists[i];

		//Copy vertex data into respective buffers
		for (int32_t j = 0; j < draw_list->VtxBuffer.Size; j++) {
			ImDrawVert* vert = draw_list->VtxBuffer.Data + j;
			uint32_t vector_offset = (j + vtx_offset) * 2 * sizeof(float);
			uint32_t int_offset = (j + vtx_offset) * sizeof(uint32_t);
			memcpy(imgui_positions.data() + vector_offset, &vert->pos, 2 * sizeof(float));
			memcpy(imgui_uvs.data() + vector_offset, &vert->uv, 2 * sizeof(float));
			memcpy(imgui_colors.data() + int_offset, &vert->col, sizeof(uint32_t));
		}

		//Copy index data
		size_t copy_size = draw_list->IdxBuffer.Size * sizeof(ImDrawIdx);
		size_t copy_offset = (idx_offset + current_index_offset) * sizeof(ImDrawIdx);
		memcpy(gpu_idx_ptr + copy_offset, draw_list->IdxBuffer.Data, copy_size);

		//Record draw commands
		for (int32_t j = 0; j < draw_list->CmdBuffer.Size; j++){
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
			
			pcs[0] = (uint32_t)draw_command.TextureId;
			vkCmdPushConstants(frame_cb, *vgd->get_pipeline_layout(graphics_pipeline_layout), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 8, pcs);

			vkCmdDrawIndexed(frame_cb, draw_command.ElemCount, 1, draw_command.IdxOffset + idx_offset, draw_command.VtxOffset + current_vertex_offset + vtx_offset, 0);
		}
		vtx_offset += draw_list->VtxBuffer.Size;
		idx_offset += draw_list->IdxBuffer.Size;
	}

	//Finally copy the intermediate vertex buffers to the real ones on the GPU
	uint32_t vector_offset = current_vertex_offset * 2 * sizeof(float);
	uint32_t int_offset = current_vertex_offset * sizeof(uint32_t);
	memcpy(gpu_pos_ptr + vector_offset, imgui_positions.data(), imgui_positions.size());
	memcpy(gpu_uv_ptr + vector_offset, imgui_uvs.data(), imgui_uvs.size());
	memcpy(gpu_col_ptr + int_offset, imgui_colors.data(), imgui_colors.size());
	
}

uint32_t ImguiRenderer::get_atlas_idx() { return atlas_idx; }