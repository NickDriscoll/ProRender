#include "Renderer.h"
#include "imgui.h"
#include "SDL.h"

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

Renderer::Renderer(VulkanGraphicsDevice* vgd) {

    //Create bindless descriptor set
    {
        {
            std::vector<VkSampler> samplers;
            {
                VkSamplerCreateInfo info = {
                    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .magFilter = VK_FILTER_LINEAR,
                    .minFilter = VK_FILTER_LINEAR,
                    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .anisotropyEnable = VK_TRUE,
                    .maxAnisotropy = 16.0,
                    .minLod = 0.0,
                    .maxLod = VK_LOD_CLAMP_NONE,
                };
                samplers.push_back(vgd->create_sampler(info));
            }

            std::vector<VulkanDescriptorLayoutBinding> bindings;
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptor_count = 1024*1024,
                .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT
            });
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptor_count = static_cast<uint32_t>(samplers.size()),
                .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .immutable_samplers = samplers.data()
            });
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptor_count = 1,
                .stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            });
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptor_count = 1,
                .stage_flags = VK_SHADER_STAGE_VERTEX_BIT
            });
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptor_count = 1,
                .stage_flags = VK_SHADER_STAGE_VERTEX_BIT
            });
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptor_count = 1,
                .stage_flags = VK_SHADER_STAGE_VERTEX_BIT
            });

            descriptor_set_layout_id = vgd->create_descriptor_set_layout(bindings);

            std::vector<VkPushConstantRange> ranges = {
                {
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = 0,
                    .size = 16
                }
            };

            pipeline_layout_id = vgd->create_pipeline_layout(descriptor_set_layout_id, ranges);
        }

        //Create bindless descriptor set
        {
            {
                VkDescriptorPoolSize sizes[] = {
                    {
                        .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                        .descriptorCount = 1024*1024,
                    },
                    {
                        .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                        .descriptorCount = 16
                    },
                    {
                        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .descriptorCount = 256
                    },
                    {
                        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .descriptorCount = 16
                    }
                };

                VkDescriptorPoolCreateInfo info = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                    .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                    .maxSets = 1,
                    .poolSizeCount = 4,
                    .pPoolSizes = sizes
                };

                if (vkCreateDescriptorPool(vgd->device, &info, vgd->alloc_callbacks, &descriptor_pool) != VK_SUCCESS) {
                    printf("Creating descriptor pool failed.\n");
                    exit(-1);
                }
            }

            {
                VkDescriptorSetAllocateInfo info = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = descriptor_pool,
                    .descriptorSetCount = 1,
                    .pSetLayouts = vgd->get_descriptor_set_layout(descriptor_set_layout_id)
                };

                if (vkAllocateDescriptorSets(vgd->device, &info, &descriptor_set) != VK_SUCCESS) {
                    printf("Allocating descriptor set failed.\n");
                    exit(-1);
                }
            }
        }
    }
    
    //Allocate memory for ImGUI vertex data
    //TODO: probabaly shouldn't be in renderer init
    {
        VkDeviceSize buffer_size = 256 * 1024;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        alloc_info.priority = 1.0;

        //imgui_vertex_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
        imgui_position_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
        imgui_uv_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
        imgui_color_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
        imgui_index_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, alloc_info);
    }

    //Create buffer for per-frame uniform data
    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    alloc_info.priority = 1.0;
    frame_uniforms_buffer = vgd->create_buffer(FRAMES_IN_FLIGHT * sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, alloc_info);

	//Bind static descriptors
	{
		std::vector<VkWriteDescriptorSet> descriptor_writes;
        
        
        VkDescriptorBufferInfo uniform_buffer_info = {
            .buffer = vgd->get_buffer(frame_uniforms_buffer)->buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };
        
        VkWriteDescriptorSet uniform_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &uniform_buffer_info
        };
        descriptor_writes.push_back(uniform_write);
        
        VkDescriptorBufferInfo im_pos_buffer_info = {
            .buffer = vgd->get_buffer(imgui_position_buffer)->buffer,
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
            .buffer = vgd->get_buffer(imgui_uv_buffer)->buffer,
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
            .buffer = vgd->get_buffer(imgui_color_buffer)->buffer,
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

    //Save pointer to graphics device
    this->vgd = vgd;
}

Renderer::~Renderer() {
    //vgd->destroy_buffer(imgui_vertex_buffer);
	vkDestroyDescriptorPool(vgd->device, descriptor_pool, vgd->alloc_callbacks);
    vgd->destroy_buffer(imgui_position_buffer);
    vgd->destroy_buffer(imgui_uv_buffer);
    vgd->destroy_buffer(imgui_color_buffer);
    vgd->destroy_buffer(imgui_index_buffer);
    vgd->destroy_buffer(frame_uniforms_buffer);
}