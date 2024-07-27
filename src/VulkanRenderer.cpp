#include "VulkanRenderer.h"
#include "imgui.h"
#include "utils.h"
#include <limits>

#define _USE_MATH_DEFINES
#include <math.h>

hlslpp::float4x4 Camera::make_view_matrix() {
    using namespace hlslpp;

	float cospitch = cosf(pitch);
	float sinpitch = sinf(pitch);
	float4x4 pitch_matrix(
		1.0, 0.0, 0.0, 0.0,
		0.0, cospitch, -sinpitch, 0.0,
		0.0, sinpitch, cospitch, 0.0,
		0.0, 0.0, 0.0, 1.0
	);

	float cosyaw = cosf(yaw);
	float sinyaw = sinf(yaw);
	float4x4 yaw_matrix(
		cosyaw, -sinyaw, 0.0, 0.0,
		sinyaw, cosyaw, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0, 0.0, 0.0, 1.0
	);

    float cosroll = cosf(roll);
    float sinroll = sinf(roll);
    float4x4 roll_matrix(
        cosroll, 0.0f, sinroll, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -sinroll, 0.0f, cosroll, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );

	float4x4 trans_matrix(
		1.0f, 0.0f, 0.0f, -position.x,
		0.0f, 1.0f, 0.0f, -position.y,
		0.0f, 0.0f, 1.0f, -position.z,
		0.0f, 0.0f, 0.0f, 1.0f
	);

    float4x4 py = mul(pitch_matrix, yaw_matrix);
    float4x4 pyr = mul(py, roll_matrix);
    return mul(pyr, trans_matrix);
}

VulkanRenderer::VulkanRenderer(VulkanGraphicsDevice* vgd, Key<VkRenderPass> window_renderpass, uint32_t rendertarget_width, uint32_t rendertarget_height) {

    cameras.alloc(MAX_CAMERAS);
    _position_buffers.alloc(MAX_VERTEX_ATTRIBS);
    _color_buffers.alloc(MAX_VERTEX_ATTRIBS);
    _uv_buffers.alloc(MAX_VERTEX_ATTRIBS);
    _index16_buffers.alloc(MAX_VERTEX_ATTRIBS);
    _materials.alloc(MAX_MATERIALS);
    _gpu_materials.alloc(MAX_MATERIALS);
    _gpu_meshes.alloc(MAX_MESHES);
    
    //Allocate memory for vertex data
    {
        VmaAllocationCreateInfo alloc_info = {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            .priority = 1.0
        };

        VkDeviceSize buffer_size = 4 * 1024 * 1024;
        vertex_position_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, alloc_info);
        vertex_color_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, alloc_info);
        vertex_uv_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, alloc_info);
        
        //Cache buffer devices addresses
        frame_uniforms.positions_addr = vgd->buffer_device_address(vertex_position_buffer);
        frame_uniforms.colors_addr = vgd->buffer_device_address(vertex_color_buffer);
        frame_uniforms.uvs_addr = vgd->buffer_device_address(vertex_uv_buffer);
        
        index_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, alloc_info);

        //Create buffer for per-frame uniform data
        frame_uniforms_buffer = vgd->create_buffer(FRAMES_IN_FLIGHT * sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, alloc_info);
        _frame_uniforms_addr = vgd->buffer_device_address(frame_uniforms_buffer);

        //Create camera buffer
        camera_buffer = vgd->create_buffer(FRAMES_IN_FLIGHT * MAX_CAMERAS * sizeof(GPUCamera), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, alloc_info);
        frame_uniforms.cameras_addr = vgd->buffer_device_address(camera_buffer);
    
        //Create material buffer
        _material_buffer = vgd->create_buffer(MAX_MATERIALS * sizeof(GPUMaterial), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, alloc_info);
        frame_uniforms.materials_addr = vgd->buffer_device_address(_material_buffer);
    
        //Create indirect draw buffer
        _indirect_draw_buffer = vgd->create_buffer(FRAMES_IN_FLIGHT * MAX_INDIRECT_DRAWS * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, alloc_info);
    
        //Create instance data buffer
        _instance_buffer = vgd->create_buffer(FRAMES_IN_FLIGHT * MAX_INSTANCES * sizeof(GPUInstanceData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, alloc_info);
        frame_uniforms.instance_data_addr = vgd->buffer_device_address(_instance_buffer);
    
        //Create mesh data buffer
        _mesh_buffer = vgd->create_buffer(MAX_MESHES * sizeof(GPUMesh), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, alloc_info);
        frame_uniforms.meshes_addr = vgd->buffer_device_address(_mesh_buffer);
    }

    //Create rendertarget buffers (color, depth)
    VkFormat color_buffer_format = VK_FORMAT_R8G8B8A8_SRGB;
    VkFormat depth_buffer_format = VK_FORMAT_D32_SFLOAT;
    {   
        VkExtent3D rendertarget_extent = {
            .width = rendertarget_width,
            .height = rendertarget_height,
            .depth = 1
        };
        VkImageCreateInfo infos[] = {
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = color_buffer_format,
                .extent = rendertarget_extent,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &vgd->graphics_queue_family_idx,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
            },
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = color_buffer_format,
                .extent = rendertarget_extent,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &vgd->graphics_queue_family_idx,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
            },
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = depth_buffer_format,
                .extent = rendertarget_extent,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 1,
                .pQueueFamilyIndices = &vgd->graphics_queue_family_idx,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
            }
        };

        Key<VulkanBindlessImage> buffers[3];
        VKASSERT_OR_CRASH(vgd->create_images(std::span(infos), buffers));
        color_buffers[0] = buffers[0];
        color_buffers[1] = buffers[1];
        depth_buffer = buffers[2];
    }

    //Update bindless descriptors to point to internal framebuffer images
    {
        std::vector<VkDescriptorImageInfo> desc_infos;

        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
            VkDescriptorImageInfo info = {
                .imageView = vgd->bindless_images.get(color_buffers[0])->vk_image.image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
            desc_infos.push_back(info);
        }
        VkDescriptorImageInfo info = {
            .imageView = vgd->bindless_images.get(depth_buffer)->vk_image.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        desc_infos.push_back(info);

        VkWriteDescriptorSet write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vgd->_image_descriptor_set,
            .dstBinding = DescriptorBindings::SAMPLED_IMAGES,
            .dstArrayElement = 0,
            .descriptorCount = (uint32_t)desc_infos.size(),
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = desc_infos.data()
        };
        vkUpdateDescriptorSets(vgd->device, 1, &write, 0, nullptr);
    }

    //Create renderpass for rendering to said rendertarget
    {
        VkAttachmentDescription2 attachments[] = {
            {
                .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                .pNext = nullptr,
                .flags = 0,
                .format = color_buffer_format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            },
            {
                .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
                .pNext = nullptr,
                .flags = 0,
                .format = depth_buffer_format,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            }
        };

		VkAttachmentReference2 color_ref = {
            .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
            .pNext = nullptr,
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT
        };
        VkAttachmentReference2 depth_ref = {
            .sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2,
            .pNext = nullptr,
            .attachment = 1,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT
        };

		VkSubpassDescription2 subpass = {
            .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
            .pNext = nullptr,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_ref,
            .pDepthStencilAttachment = &depth_ref
        };

        VkSubpassDependency2 dep = {
            .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2,
            .pNext = nullptr,
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            .srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = 0,
            .viewOffset = 0
        };

		VkRenderPassCreateInfo2 info = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .attachmentCount = 2,
            .pAttachments = attachments,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dep
        };
		
		main_framebuffers[0].render_pass = vgd->create_render_pass(info);
        main_framebuffers[1].render_pass = main_framebuffers[0].render_pass;
	}

    //Create rendertarget framebuffers
    {
        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i) {
            VkImageView views[] = {
                vgd->bindless_images.get(color_buffers[i])->vk_image.image_view,
                vgd->bindless_images.get(depth_buffer)->vk_image.image_view
            };  

            VkFramebufferCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.renderPass = *vgd->get_render_pass(main_framebuffers[i].render_pass);
            info.attachmentCount = 2;
            info.pAttachments = views;
            info.width = rendertarget_width;
            info.height = rendertarget_height;
            info.layers = 1;

            VkClearValue clear_color;
            clear_color.color.float32[0] = 0.0f;
            clear_color.color.float32[1] = 0.0f;
            clear_color.color.float32[2] = 0.0f;
            clear_color.color.float32[3] = 1.0f;
            main_framebuffers[i].clear_values.push_back(clear_color);

            clear_color.depthStencil.depth = 0.0f;
            clear_color.depthStencil.stencil = 0;
            main_framebuffers[i].clear_values.push_back(clear_color);

            main_framebuffers[i].width = info.width;
            main_framebuffers[i].height = info.height;
            main_framebuffers[i].fb = vgd->create_framebuffer(info);
        }
	}

    //Create hardcoded graphics pipelines
	{
        
		const char* ps1_spv[] = { "shaders/ps1.vert.spv", "shaders/ps1.frag.spv" };
        VulkanGraphicsPipelineConfig ps1_config = VulkanGraphicsPipelineConfig();
        // config.rasterization_state.cullMode = VK_CULL_MODE_NONE;
        // config.depth_stencil_state.depthTestEnable = VK_FALSE;
        ps1_config.depth_stencil_state.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
        ps1_config.render_pass = main_framebuffers[0].render_pass;
        ps1_config.spv_sources = ps1_spv;

        const char* postfx_spv[] = { "shaders/postfx.vert.spv", "shaders/postfx.frag.spv" };
        VulkanGraphicsPipelineConfig postfx_config = VulkanGraphicsPipelineConfig();
        postfx_config.render_pass = window_renderpass;
        postfx_config.spv_sources = postfx_spv;

        std::vector<VulkanGraphicsPipelineConfig> configs = {ps1_config, postfx_config};
		Key<VulkanGraphicsPipeline> pipelines[] = {0, 0};
		vgd->create_graphics_pipelines(
            configs,
			pipelines
		);

		ps1_pipeline = pipelines[0];
        postfx_pipeline = pipelines[1];
	}

	//Create graphics pipeline timeline semaphore
	frames_completed_semaphore = vgd->create_timeline_semaphore(0);

    //Save pointer to graphics device
    this->vgd = vgd;
}

Key<BufferView> VulkanRenderer::push_vertex_positions(std::span<float> data) {
    VulkanBuffer* buffer = vgd->get_buffer(vertex_position_buffer);
    float* ptr = (float*)buffer->alloc_info.pMappedData;
    ptr += vertex_position_offset;
    memcpy(ptr, data.data(), data.size_bytes());

    BufferView b = {
        .start = vertex_position_offset,
        .length = (uint32_t)data.size()
    };

    vertex_position_offset += (uint32_t)data.size();
    
    return _position_buffers.insert(b);
}

BufferView* VulkanRenderer::get_vertex_positions(Key<BufferView> key) {
    return _position_buffers.get(key);
}

Key<MeshAttribute> VulkanRenderer::push_vertex_colors(Key<BufferView> position_key, std::span<float> data) {
    PRORENDER_ASSERT(_position_buffers.get(position_key) != nullptr, true);
    
    VulkanBuffer* buffer = vgd->get_buffer(vertex_color_buffer);
    float* ptr = (float*)buffer->alloc_info.pMappedData;
    ptr += vertex_color_offset;
    memcpy(ptr, data.data(), data.size_bytes());

    BufferView b = {
        .start = vertex_color_offset,
        .length = (uint32_t)data.size()
    };

    vertex_color_offset += (uint32_t)data.size();

    MeshAttribute a = {
        .position_key = position_key,
        .view = b
    };
    return _color_buffers.insert(a);
}

BufferView* VulkanRenderer::get_vertex_colors(Key<BufferView> key) {

    BufferView* result = nullptr;
    for (MeshAttribute& att : _color_buffers) {
        if (att.position_key.value() == key.value()) {
            result = &att.view;
            break;
        }
    }

    return result;
}

Key<MeshAttribute> VulkanRenderer::push_vertex_uvs(Key<BufferView> position_key, std::span<float> data) {
    PRORENDER_ASSERT(_position_buffers.get(position_key) != nullptr, true);
    
    VulkanBuffer* buffer = vgd->get_buffer(vertex_uv_buffer);
    float* ptr = (float*)buffer->alloc_info.pMappedData;
    ptr += vertex_uv_offset;
    memcpy(ptr, data.data(), data.size_bytes());

    BufferView b = {
        .start = vertex_uv_offset,
        .length = (uint32_t)data.size()
    };

    vertex_uv_offset += (uint32_t)data.size();

    MeshAttribute a = {
        .position_key = position_key,
        .view = b
    };
    return _uv_buffers.insert(a);
}

//TODO: Just kind of accepting the O(n) lookup because of hand-waving about cache
//TODO: Make common function out of this and get_index16
BufferView* VulkanRenderer::get_vertex_uvs(Key<BufferView> position_key) {

    BufferView* result = nullptr;
    for (MeshAttribute& att : _uv_buffers) {
        if (att.position_key.value() == position_key.value()) {
            result = &att.view;
            break;
        }
    }

    return result;
}

Key<MeshAttribute> VulkanRenderer::push_indices16(Key<BufferView> position_key, std::span<uint16_t> data) {
    PRORENDER_ASSERT(_position_buffers.get(position_key) != nullptr, true);

    VulkanBuffer* buffer = vgd->get_buffer(index_buffer);
    uint16_t* ptr = (uint16_t*)buffer->alloc_info.pMappedData;
    ptr += index_buffer_offset;
    memcpy(ptr, data.data(), data.size_bytes());

    BufferView b = {
        .start = index_buffer_offset,
        .length = (uint32_t) data.size()
    };

    index_buffer_offset += (uint32_t)data.size();

    MeshAttribute a = {
        .position_key = position_key,
        .view = b
    };
    return _index16_buffers.insert(a);
}

BufferView* VulkanRenderer::get_indices16(Key<BufferView> position_key) {

    BufferView* result = nullptr;
    for (MeshAttribute& att : _index16_buffers) {
        if (att.position_key.value() == position_key.value()) {
            result = &att.view;
            break;
        }
    }

    return result;

}

Key<Material> VulkanRenderer::push_material(uint32_t sampler_idx, const hlslpp::float4& base_color) {
    return this->push_material(0, sampler_idx, base_color);
}

Key<Material> VulkanRenderer::push_material(uint64_t batch_id, uint32_t sampler_idx, const hlslpp::float4& base_color) {
    Material mat {
        .base_color = base_color,
        .batch_id = batch_id,
        .sampler_idx = sampler_idx
    };
    return _materials.insert(mat);
}

uint64_t VulkanRenderer::get_current_frame() {
    return _current_frame;
}

//Records one indirect draw command into the ps1 draws queue
void VulkanRenderer::ps1_draw(Key<BufferView> mesh_key, Key<Material> material_key, const std::span<InstanceData>& instance_datas) {
    Material* material = _materials.get(material_key);
    if (material->batch_id > vgd->completed_image_batches()) return;     //Early exit if material's batch hasn't completed
    
    // {
    //     static int last_seen = 0;
    //     if (material->batch_id > last_seen) {
    //         last_seen = (int)material->batch_id;
    //         printf("Saw material with batch id %i with bindless_images having %i images..\n", (int)material->batch_id, (int)vgd->bindless_images.count());
    //     }
    // }

    //Check if this material's images have already been loaded
    Key<GPUMaterial> gpu_mat_key;
    if (_material_map.contains(material_key.value())) {
        //If yes, reuse that data
        gpu_mat_key = _material_map[material_key.value()];
    } else {
        //printf("Drawing material that has not been loaded before from batch %i...\n", (int)material->batch_id);
        //Otherwise we have to search for the image in the available images array
        //and upload its metadata to the GPU
        _material_dirty_flag = true;
        
        GPUMaterial mat;
        mat.sampler_idx = material->sampler_idx;
        mat.base_color = material->base_color;

        if (material->batch_id > 0) {
            for (auto it = vgd->bindless_images.begin(); it != vgd->bindless_images.end(); ++it) {
                VulkanBindlessImage& image = *it;
                //printf("Image batch id: %d\nMaterial batch id:%d\n", (int)image.batch_id, (int)material->batch_id);
                if (image.batch_id == material->batch_id) {
                    mat.texture_indices[0] = it.slot_index();
                    //printf("Match!\n");
                    break;
                }
                //printf("\n");
            }
            assert(mat.texture_indices[0] != std::numeric_limits<uint32_t>::max());
        } else {
            //Material doesn't have textures, so use defaults
            mat.texture_indices[0] = std::numeric_limits<uint32_t>::max();
        }

        gpu_mat_key = _gpu_materials.insert(mat);
        _material_map.insert(std::pair(material_key.value(), gpu_mat_key.value()));
    }

    //Get geometry data
    BufferView* index_data = get_indices16(mesh_key);
    Key<GPUMesh> gpu_mesh_key;
    if (_mesh_map.contains(mesh_key.value())) {
        gpu_mesh_key = _mesh_map[mesh_key.value()];
    } else {
        _mesh_dirty_flag = true;
        BufferView* position_data = _position_buffers.get(mesh_key);
        BufferView* uv_data = get_vertex_uvs(mesh_key);
        BufferView* color_data = get_vertex_colors(mesh_key);

        if (position_data == nullptr) return;

        uint32_t uv_start = 0xFFFFFFFF;
        if (uv_data != nullptr) uv_start = uv_data->start;

        uint32_t color_start = 0xFFFFFFFF;
        if (color_data != nullptr) color_start = color_data->start;

        GPUMesh g_mesh = {
            .position_start = position_data->start,
            .uv_start = uv_start,
            .color_start = color_start
        };
        gpu_mesh_key = _gpu_meshes.insert(g_mesh);
        _mesh_map.insert(std::pair(mesh_key.value(), gpu_mesh_key.value()));
    }

    //Record GPUInstanceData structure(s)
    uint32_t instance_count = (uint32_t)instance_datas.size();
    for (InstanceData& in_data : instance_datas) {
        GPUInstanceData g_data = {
            .world_matrix = in_data.world_from_model,
            .mesh_idx = EXTRACT_IDX(gpu_mesh_key.value()),
            .material_idx = EXTRACT_IDX(gpu_mat_key.value()),
        };
        _gpu_instance_datas.push_back(g_data);
    }

    //Finally, record the actual indirect draw command
    uint32_t in_flight_frame_slot = _current_frame % FRAMES_IN_FLIGHT;
    VkDrawIndexedIndirectCommand command = {
        .indexCount = index_data->length,
        .instanceCount = instance_count,
        .firstIndex = index_data->start,
        .vertexOffset = 0,
        .firstInstance = _instances_so_far + MAX_INSTANCES * in_flight_frame_slot
    };
    _instances_so_far += instance_count;
    _draw_calls.push_back(command);
}

//Synchronizes CPU and GPU buffers, then
//records and submits all rendering commands in frame_cb
void VulkanRenderer::render(VkCommandBuffer frame_cb, SyncData& sync_data) {
    //Upload material buffer if it changed
    if (_material_dirty_flag) {
        _material_dirty_flag = false;

        VulkanBuffer* mat_buffer = vgd->get_buffer(_material_buffer);
        GPUMaterial* ptr = static_cast<GPUMaterial*>(mat_buffer->alloc_info.pMappedData);
        memcpy(ptr, _gpu_materials.data(), _gpu_materials.size() * sizeof(GPUMaterial));
    }

    //Upload mesh buffer if it changed
    if (_mesh_dirty_flag) {
        _mesh_dirty_flag = false;

        VulkanBuffer* mesh_buffer = vgd->get_buffer(_mesh_buffer);
        GPUMesh* ptr = static_cast<GPUMesh*>(mesh_buffer->alloc_info.pMappedData);
        memcpy(ptr, _gpu_meshes.data(), _gpu_meshes.size() * sizeof(GPUMesh));
    }

    //Update per-frame uniforms
    //TODO: This is currently doing nothing to account for multiple in-flight frames
    {
        VulkanBuffer* uniform_buffer = vgd->get_buffer(frame_uniforms_buffer);
        memcpy(uniform_buffer->alloc_info.pMappedData, &frame_uniforms, sizeof(FrameUniforms));
    }

    //Upload instance data buffer
    {
        VulkanBuffer* instance_buffer = vgd->get_buffer(_instance_buffer);
        GPUInstanceData* ptr = static_cast<GPUInstanceData*>(instance_buffer->alloc_info.pMappedData);
        ptr += (_current_frame % FRAMES_IN_FLIGHT) * MAX_INSTANCES;
        memcpy(ptr, _gpu_instance_datas.data(), _gpu_instance_datas.size() * sizeof(GPUInstanceData));
    }

    //Upload indirect draw buffer
    {
        VulkanBuffer* indirect_draw_buffer = vgd->get_buffer(_indirect_draw_buffer);
        VkDrawIndexedIndirectCommand* ptr = static_cast<VkDrawIndexedIndirectCommand*>(indirect_draw_buffer->alloc_info.pMappedData);
        ptr += (_current_frame % FRAMES_IN_FLIGHT) * MAX_INDIRECT_DRAWS;
        memcpy(ptr, _draw_calls.data(), _draw_calls.size() * sizeof(VkDrawIndexedIndirectCommand));
    }

    VulkanFrameBuffer& main_framebuffer = main_framebuffers[_current_frame % FRAMES_IN_FLIGHT];

    //Update GPU camera data
    std::vector<uint32_t> cam_idx_map;
    cam_idx_map.reserve(cameras.count());
    {
        using namespace hlslpp;

        std::vector<GPUCamera> g_cameras;
        g_cameras.reserve(cameras.count());

        for (auto it = cameras.begin(); it != cameras.end(); ++it) {
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

            float aspect = (float)main_framebuffer.width / (float)main_framebuffer.height;
            float desired_fov = (float)(M_PI / 2.0);
            float nearplane = 0.1f;
            float farplane = 1000000.0f;
            float tan_fovy = tanf(desired_fov / 2.0f);
            gcam.projection_matrix = float4x4(
                1.0f / (tan_fovy * aspect), 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f / tan_fovy, 0.0f, 0.0f,
                0.0f, 0.0f, nearplane / (nearplane - farplane), (nearplane * farplane) / (farplane - nearplane),
                0.0f, 0.0f, 1.0f, 0.0f
            );
            gcam.projection_matrix = mul(gcam.projection_matrix, c_matrix);

            g_cameras.push_back(gcam);
            cam_idx_map.push_back(it.slot_index());
        }

        //Write camera data to GPU buffer in one contiguous push
        //TODO: This is currently doing nothing to account for multiple in-flight frames
        VulkanBuffer* cam_buffer = vgd->get_buffer(camera_buffer);
        memcpy(cam_buffer->alloc_info.pMappedData, g_cameras.data(), g_cameras.size() * sizeof(GPUCamera));
    }

    {	
        vgd->begin_render_pass(frame_cb, main_framebuffer);

		//Set viewport and scissor
		{
			VkViewport viewport = {
				.x = 0,
				.y = 0,
				.width = (float)main_framebuffer.width,
				.height = (float)main_framebuffer.height,
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
					.width = main_framebuffer.width,
					.height = main_framebuffer.height
				}
			};
			vkCmdSetScissor(frame_cb, 0, 1, &scissor);
		}

		//Bind global index buffer
		vkCmdBindIndexBuffer(frame_cb, vgd->get_buffer(index_buffer)->buffer, 0, VK_INDEX_TYPE_UINT16);
		
		//Bind pipeline for this pass
		vkCmdBindPipeline(frame_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vgd->get_graphics_pipeline(ps1_pipeline)->pipeline);

        //Bind push constants for this pass
        RenderPushConstants pcs = {
            .uniforms_addr = _frame_uniforms_addr,
            .camera_idx = 0
        };
        vkCmdPushConstants(frame_cb, vgd->get_pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(RenderPushConstants), &pcs);

        VkDeviceSize indirect_offset = (_current_frame % FRAMES_IN_FLIGHT) * MAX_INDIRECT_DRAWS * sizeof(VkDrawIndexedIndirectCommand);
        vkCmdDrawIndexedIndirect(frame_cb, vgd->get_buffer(_indirect_draw_buffer)->buffer, indirect_offset, static_cast<uint32_t>(_draw_calls.size()), sizeof(VkDrawIndexedIndirectCommand));

        vgd->end_render_pass(frame_cb);

        //Barrier so that rendered frame becomes available to later stages
        // {
        //     VkImageMemoryBarrier2KHR barrier = {
        //         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
        //         .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
        //         .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
        //         .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
        //         .dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,

        //         .oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        //         .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        //         .srcQueueFamilyIndex = vgd->graphics_queue_family_idx,
        //         .dstQueueFamilyIndex = vgd->graphics_queue_family_idx,
        //         .image = vgd->bindless_images.get(color_buffers[_current_frame % FRAMES_IN_FLIGHT])->vk_image.image,
        //         .subresourceRange = {
        //             .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        //             .baseMipLevel = 0,
        //             .levelCount = 1,
        //             .baseArrayLayer = 0,
        //             .layerCount = 1
        //         }
        //     };

        //     VkDependencyInfoKHR info = {};
        //     info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        //     info.imageMemoryBarrierCount = 1;
        //     info.pImageMemoryBarriers = &barrier;

        //     vkCmdPipelineBarrier2KHR(frame_cb, &info);
        // }

        sync_data.signal_semaphores.push_back(*vgd->get_semaphore(frames_completed_semaphore));
        sync_data.signal_values.push_back(_current_frame + 1);

        if (_current_frame >= FRAMES_IN_FLIGHT) {
            sync_data.cpu_wait_semaphore = *vgd->get_semaphore(frames_completed_semaphore);
            sync_data.cpu_wait_value = _current_frame - FRAMES_IN_FLIGHT + 1;
        }
	}

    //Get renderer state ready for next frame
    _draw_calls.clear();
    _gpu_instance_datas.clear();
    _instances_so_far = 0;
    _current_frame += 1;
}

void VulkanRenderer::postprocessing(VkCommandBuffer frame_cb, VulkanFrameBuffer& framebuffer) {
	VkViewport viewport = {
		.x = 0,
		.y = 0,
		.width = (float)framebuffer.width,
		.height = (float)framebuffer.height,
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
            .width = framebuffer.width,
            .height = framebuffer.height
        }
    };
    vkCmdSetScissor(frame_cb, 0, 1, &scissor);

    uint32_t color_buffer_idx = EXTRACT_IDX(color_buffers[_current_frame % FRAMES_IN_FLIGHT].value());
    vkCmdPushConstants(frame_cb, vgd->get_pipeline_layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &color_buffer_idx);
    vkCmdBindPipeline(frame_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vgd->get_graphics_pipeline(postfx_pipeline)->pipeline);
    vkCmdDraw(frame_cb, 3, 1, 0, 0);
}

void VulkanRenderer::cpu_sync() {
    //Wait for command buffer to finish execution before submitting
	//Prevents CPU from getting too far ahead
	if (_current_frame >= FRAMES_IN_FLIGHT) {
        VkSemaphore* graphics_tl_sem = vgd->get_semaphore(frames_completed_semaphore);
        uint64_t wait_value = _current_frame - FRAMES_IN_FLIGHT + 1;
		VkSemaphoreWaitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
		info.semaphoreCount = 1;
		info.pSemaphores = graphics_tl_sem;
		info.pValues = &wait_value;
		VKASSERT_OR_CRASH(vkWaitSemaphores(vgd->device, &info, std::numeric_limits<uint64_t>::max()));
	}
}

VulkanRenderer::~VulkanRenderer() {
    vgd->destroy_buffer(_instance_buffer);
    vgd->destroy_buffer(_mesh_buffer);
    vgd->destroy_buffer(_indirect_draw_buffer);
    vgd->destroy_buffer(_material_buffer);
    vgd->destroy_buffer(camera_buffer);
    vgd->destroy_buffer(frame_uniforms_buffer);
    vgd->destroy_buffer(vertex_color_buffer);
    vgd->destroy_buffer(vertex_position_buffer);
    vgd->destroy_buffer(vertex_uv_buffer);
    vgd->destroy_buffer(index_buffer);
}