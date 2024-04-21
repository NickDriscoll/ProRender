#include "VulkanRenderer.h"
#include "imgui.h"
#include <limits>

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

VulkanRenderer::VulkanRenderer(VulkanGraphicsDevice* vgd, Key<VkRenderPass> swapchain_renderpass) {

    cameras.alloc(MAX_CAMERAS);
    _position_buffers.alloc(MAX_VERTEX_ATTRIBS);
    _uv_buffers.alloc(MAX_VERTEX_ATTRIBS);
    _index16_buffers.alloc(MAX_VERTEX_ATTRIBS);
    _materials.alloc(MAX_MATERIALS);
    _gpu_materials.alloc(MAX_MATERIALS);
    _gpu_meshes.alloc(MAX_MESHES);

    //Create bindless descriptor set
    {
        {
            //Create immutable samplers
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
                _samplers.push_back(vgd->create_sampler(info));
                standard_sampler_idx = 0;

                info = {
                    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .magFilter = VK_FILTER_NEAREST,
                    .minFilter = VK_FILTER_NEAREST,
                    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                    .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    .anisotropyEnable = VK_FALSE,
                    .maxAnisotropy = 1.0,
                    .minLod = 0.0,
                    .maxLod = VK_LOD_CLAMP_NONE,
                };
                _samplers.push_back(vgd->create_sampler(info));
                point_sampler_idx = 1;
            }

            std::vector<VulkanDescriptorLayoutBinding> bindings;

            //Images
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptor_count = 1024*1024,
                .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT
            });

            //Samplers
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptor_count = static_cast<uint32_t>(_samplers.size()),
                .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .immutable_samplers = _samplers.data()
            });

            //Frame uniforms
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptor_count = 1,
                .stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            });

            //Imgui positions
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptor_count = 1,
                .stage_flags = VK_SHADER_STAGE_VERTEX_BIT
            });

            //Imgui UVs
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptor_count = 1,
                .stage_flags = VK_SHADER_STAGE_VERTEX_BIT
            });

            //Imgui colors
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptor_count = 1,
                .stage_flags = VK_SHADER_STAGE_VERTEX_BIT
            });

            //Vertex positions
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptor_count = 1,
                .stage_flags = VK_SHADER_STAGE_VERTEX_BIT
            });

            //Vertex uvs
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptor_count = 1,
                .stage_flags = VK_SHADER_STAGE_VERTEX_BIT
            });

            //Camera buffer
            bindings.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptor_count = 1,
                .stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            });

            descriptor_set_layout_id = vgd->create_descriptor_set_layout(bindings);

            std::vector<VkPushConstantRange> ranges = {
                {
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                    .offset = 0,
                    .size = 20
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
    
    //Allocate memory for vertex data
    {
        VmaAllocationCreateInfo alloc_info = {
            .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
            .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            .priority = 1.0
        };

        VkDeviceSize buffer_size = 1024 * 1024;
        vertex_position_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
        vertex_uv_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
        index_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, alloc_info);
    }

    //Create buffer for per-frame uniform data
    {
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        alloc_info.priority = 1.0;
        frame_uniforms_buffer = vgd->create_buffer(FRAMES_IN_FLIGHT * sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, alloc_info);
    }

    //Create camera buffer
    {
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        alloc_info.priority = 1.0;
        camera_buffer = vgd->create_buffer(FRAMES_IN_FLIGHT * MAX_CAMERAS * sizeof(GPUCamera), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
    }

    //Create material buffer
    {
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        alloc_info.priority = 1.0;
        _material_buffer = vgd->create_buffer(MAX_MATERIALS * sizeof(GPUMaterial), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
    }

    //Create indirect draw buffer
    {
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        alloc_info.priority = 1.0;
        _indirect_draw_buffer = vgd->create_buffer(FRAMES_IN_FLIGHT * MAX_INDIRECT_DRAWS * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
    }

    //Create instance data buffer
    {
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        alloc_info.priority = 1.0;
        _instance_buffer = vgd->create_buffer(FRAMES_IN_FLIGHT * MAX_INSTANCES * sizeof(GPUInstanceData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
    }

    //Create mesh data buffer
    {
        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        alloc_info.priority = 1.0;
        _mesh_buffer = vgd->create_buffer(MAX_MESHES * sizeof(GPUMesh), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
    }

	//Write static descriptors
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
        
        VkDescriptorBufferInfo vert_pos_buffer_info = {
            .buffer = vgd->get_buffer(vertex_position_buffer)->buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };
        
        VkWriteDescriptorSet vert_pos_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 6,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &vert_pos_buffer_info
        };
        descriptor_writes.push_back(vert_pos_write);
        
        VkDescriptorBufferInfo vert_uv_buffer_info = {
            .buffer = vgd->get_buffer(vertex_uv_buffer)->buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };
        
        VkWriteDescriptorSet vert_uv_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 7,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &vert_uv_buffer_info
        };
        descriptor_writes.push_back(vert_uv_write);
        
        VkDescriptorBufferInfo camera_buffer_info = {
            .buffer = vgd->get_buffer(camera_buffer)->buffer,
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };
        
        VkWriteDescriptorSet camera_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 8,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &camera_buffer_info
        };
        descriptor_writes.push_back(camera_write);

        vkUpdateDescriptorSets(vgd->device, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);
	}

    //Create hardcoded graphics pipelines
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
			{}
		};

		VulkanMultisampleState ms_states[] = {
			{}
		};

		VulkanDepthStencilState ds_states[] = {
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

		const char* shaders[] = { "shaders/ps1.vert.spv", "shaders/ps1.frag.spv" };

		Key<VulkanGraphicsPipeline> pipelines[] = {0};
		vgd->create_graphics_pipelines(
			pipeline_layout_id,
			swapchain_renderpass,
			shaders,
			ia_states,
			tess_states,
			vs_states,
			rast_states,
			ms_states,
			ds_states,
			blend_states,
			pipelines,
			1
		);

		ps1_pipeline = pipelines[0];
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

Key<MeshAttribute> VulkanRenderer::push_vertex_uvs(Key<BufferView> position_key, std::span<float> data) {
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

Key<Material> VulkanRenderer::push_material(uint64_t batch_id, hlslpp::float4& base_color) {
    Material mat {
        .batch_id = batch_id,
        .base_color = base_color,
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

    //Check if this material's images have already been loaded
    Key<GPUMaterial> gpu_mat_key;
    if (_material_map.contains(material_key.value())) {
        //If yes, reuse that data
        gpu_mat_key = _material_map[material_key.value()];
    } else {
        //Otherwise we have to search for the image in the available images array
        //and upload its metadata to the GPU
        _material_dirty_flag = true;
        
        GPUMaterial mat;
        mat.sampler_idx = material->sampler_idx;
        mat.base_color = material->base_color;

        for (auto it = vgd->available_images.begin(); it != vgd->available_images.end(); ++it) {
            VulkanAvailableImage& image = *it;
            if (image.batch_id == material->batch_id) {
                mat.texture_indices[0] = it.slot_index();
            }
        }
        assert(mat.texture_indices[0] != std::numeric_limits<uint32_t>::max());

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
        GPUMesh g_mesh = {
            .position_start = position_data->start,
            .uv_start = uv_data->start
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
    VkDrawIndexedIndirectCommand command = {
        .indexCount = index_data->length,
        .instanceCount = instance_count,
        .firstIndex = index_data->start,
        .vertexOffset = 0,                   //TODO: Eliminate GPUMeshData?
        .firstInstance = _instances_so_far
    };
    _instances_so_far += instance_count;
    _draw_calls.push_back(command);

}

//Synchronizes CPU and GPU buffers, then
//records and submits all rendering commands in one command buffer
void VulkanRenderer::render(VkCommandBuffer frame_cb, VulkanFrameBuffer& framebuffer, SyncData& sync_data) {

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

    //Upload instance data buffer
    {
        VulkanBuffer* instance_buffer = vgd->get_buffer(_instance_buffer);
        GPUInstanceData* ptr = static_cast<GPUInstanceData*>(instance_buffer->alloc_info.pMappedData);
        ptr += (_current_frame % FRAMES_IN_FLIGHT) * MAX_INSTANCES;
        printf("Writing to ptr 0x%llX\n", ptr);
        printf("Instance data size: %i\n", _gpu_instance_datas.size());
        memcpy(ptr, _gpu_instance_datas.data(), _gpu_instance_datas.size() * sizeof(GPUInstanceData));
    }

    //Upload indirect draw buffer
    {
        VulkanBuffer* indirect_draw_buffer = vgd->get_buffer(_indirect_draw_buffer);
        VkDrawIndexedIndirectCommand* ptr = static_cast<VkDrawIndexedIndirectCommand*>(indirect_draw_buffer->alloc_info.pMappedData);
        ptr += (_current_frame % FRAMES_IN_FLIGHT) * MAX_INDIRECT_DRAWS;
        memcpy(ptr, _draw_calls.data(), _draw_calls.size() * sizeof(VkDrawIndexedIndirectCommand));
    }

    {
		//Wait for command buffer to finish execution before trying to record to it
		if (_current_frame >= FRAMES_IN_FLIGHT) {
			uint64_t wait_value = _current_frame - FRAMES_IN_FLIGHT + 1;

			VkSemaphoreWaitInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
			info.semaphoreCount = 1;
			info.pSemaphores = vgd->get_semaphore(frames_completed_semaphore);
			info.pValues = &wait_value;
			if (vkWaitSemaphores(vgd->device, &info, std::numeric_limits<uint64_t>::max()) != VK_SUCCESS) {
				printf("Waiting for graphics timeline semaphore failed.\n");
				exit(-1);
			}
		}
		
		uint64_t in_flight_frame = _current_frame % FRAMES_IN_FLIGHT;

		//Acquire swapchain image for this frame
		//We want to do this as soon as possible
		// uint32_t acquired_image_idx;
		// vkAcquireNextImageKHR(vgd->device, window.swapchain, std::numeric_limits<uint64_t>::max(), window.acquire_semaphores[in_flight_frame], VK_NULL_HANDLE, &acquired_image_idx);

		//VkCommandBuffer frame_cb = vgd->command_buffers[in_flight_frame];

		// VkCommandBufferBeginInfo begin_info = {
		// 	.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		// 	.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		// };
		// vkBeginCommandBuffer(frame_cb, &begin_info);

		//Per-frame checking of pending images to see if they're ready
		vgd->tick_image_uploads(frame_cb, descriptor_set, DescriptorBindings::SAMPLED_IMAGES);
		uint64_t upload_batches_completed = vgd->completed_image_batches();

		//Begin render pass
		{
			VkRect2D area = {
				.offset = {
					.x = 0,
					.y = 0
				},
				.extent = {
					.width = framebuffer.width,
					.height = framebuffer.height
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
			info.renderPass = *vgd->get_render_pass(framebuffer.render_pass);
            info.framebuffer = *vgd->get_framebuffer(framebuffer.fb);
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
		}

		vkCmdBindDescriptorSets(frame_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, *vgd->get_pipeline_layout(pipeline_layout_id), 0, 1, &descriptor_set, 0, nullptr);

		//Bind global index buffer
		vkCmdBindIndexBuffer(frame_cb, vgd->get_buffer(index_buffer)->buffer, 0, VK_INDEX_TYPE_UINT16);
		
		//Bind pipeline for this pass
		vkCmdBindPipeline(frame_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vgd->get_graphics_pipeline(ps1_pipeline)->pipeline);

		//Draw hardcoded plane
		// {
		// 	//Find GPU index of main viewport camera
		// 	uint32_t main_cam_idx;
		// 	{
		// 		for (uint32_t i = 0; i < cam_idx_map.size(); ++i) {
		// 			if (cam_idx_map[i] == EXTRACT_IDX(main_viewport_camera.value())) {
		// 				main_cam_idx = i;
		// 				break;
		// 			}
		// 		}
		// 	}

		// 	uint32_t image_idx = imgui_renderer.get_atlas_idx();
		// 	if (plane_image_idx != 0xFFFFFFFF)
		// 		image_idx = plane_image_idx;

		// 	BufferView* plane_positions = renderer.get_vertex_positions(plane_mesh_key);
		// 	BufferView* plane_uvs = renderer.get_vertex_uvs(plane_mesh_key);
		// 	BufferView* plane_indices = renderer.get_indices16(plane_mesh_key);

		// 	uint32_t pcs[] = {
		// 		plane_positions->start / 4,
		// 		plane_uvs->start / 2,
		// 		main_cam_idx,
		// 		image_idx,
		// 		renderer.standard_sampler_idx
		// 	};
		// 	vkCmdPushConstants(frame_cb, *vgd->get_pipeline_layout(renderer.pipeline_layout_id), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 20, pcs);

		// 	vkCmdDrawIndexed(frame_cb, plane_indices->length, 1, plane_indices->start, 0, 0);
		// }


		//Record imgui drawing commands into this frame's command buffer
		//imgui_renderer.draw(frame_cb, current_frame);

		vkCmdEndRenderPass(frame_cb);
		//vkEndCommandBuffer(frame_cb);

		//Submit rendering command buffer
        // SyncData sems = {
        //     .wait_values = {0, upload_batches_completed},
        //     .signal_values = {_current_frame + 1, 0},
        //     .wait_semaphores = {window.acquire_semaphores[in_flight_frame], vgd->image_upload_semaphore},
        //     .signal_semaphores = { graphics_timeline_semaphore, *vgd->get_semaphore(semaphore) },
        //     .count = 2,
        // }

        sync_data.signal_semaphores.push_back(*vgd->get_semaphore(frames_completed_semaphore));
        sync_data.signal_values.push_back(_current_frame + 1);

		// VkQueue q;
		// vkGetDeviceQueue(vgd->device, vgd->graphics_queue_family_idx, 0, &q);
		// {
		// 	uint64_t wait_values[] = {0, upload_batches_completed};
		// 	uint64_t signal_values[] = {_current_frame + 1, 0};
		// 	VkTimelineSemaphoreSubmitInfo ts_info = {};
		// 	ts_info.waitSemaphoreValueCount = 2;
		// 	ts_info.pWaitSemaphoreValues = wait_values;
		// 	ts_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		// 	ts_info.signalSemaphoreValueCount = 2;
		// 	ts_info.pSignalSemaphoreValues = signal_values;

		// 	VkPipelineStageFlags wait_flags[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };

		// 	VkSubmitInfo info = {};
		// 	VkSemaphore signal_semaphores[] = { graphics_timeline_semaphore, *vgd->get_semaphore(semaphore) };
		// 	VkSemaphore wait_semaphores[] = {window.acquire_semaphores[in_flight_frame], vgd->image_upload_semaphore};
		// 	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		// 	info.pNext = &ts_info;
		// 	info.waitSemaphoreCount = 2;
		// 	info.pWaitSemaphores = wait_semaphores;
		// 	info.pWaitDstStageMask = wait_flags;
		// 	info.signalSemaphoreCount = 2;
		// 	info.pSignalSemaphores = signal_semaphores;
		// 	info.commandBufferCount = 1;
		// 	info.pCommandBuffers = &frame_cb;

		// 	if (vkQueueSubmit(q, 1, &info, VK_NULL_HANDLE) != VK_SUCCESS) {
		// 		printf("Queue submit failed.\n");
		// 		exit(-1);
		// 	}
		// }

		//Queue present
		// {
		// 	VkPresentInfoKHR info = {};
		// 	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		// 	info.waitSemaphoreCount = 1;
		// 	info.pWaitSemaphores = &window.present_semaphores[in_flight_frame];
		// 	info.swapchainCount = 1;
		// 	info.pSwapchains = &window.swapchain;
		// 	info.pImageIndices = &acquired_image_idx;
		// 	info.pResults = VK_NULL_HANDLE;

		// 	VkResult r = vkQueuePresentKHR(q, &info);
		// 	switch (r) {
		// 		case VK_SUBOPTIMAL_KHR:
		// 			printf("Swapchain suboptimal.\n");
		// 			break;
		// 		case VK_ERROR_OUT_OF_DATE_KHR:
		// 			printf("Swapchain out of date.\n");
		// 			window.resize(vgd);
		// 			break;
		// 		case VK_SUCCESS:
		// 			break;
		// 		default:
		// 			printf("Queue present failed.\n");
		// 			exit(-1);
		// 			break;
		// 	}
		// }
	}




    //Get renderer state ready for next frame
    _draw_calls.clear();
    _gpu_instance_datas.clear();
    _instances_so_far = 0;
    _current_frame += 1;
}

VulkanRenderer::~VulkanRenderer() {
	for (uint32_t i = 0; i < _samplers.size(); i++) {
		vkDestroySampler(vgd->device, _samplers[i], vgd->alloc_callbacks);
	}

	vkDestroySemaphore(vgd->device, *vgd->get_semaphore(frames_completed_semaphore), vgd->alloc_callbacks);

	vkDestroyDescriptorPool(vgd->device, descriptor_pool, vgd->alloc_callbacks);
    vgd->destroy_buffer(_instance_buffer);
    vgd->destroy_buffer(_mesh_buffer);
    vgd->destroy_buffer(_indirect_draw_buffer);
    vgd->destroy_buffer(_material_buffer);
    vgd->destroy_buffer(camera_buffer);
    vgd->destroy_buffer(frame_uniforms_buffer);
    vgd->destroy_buffer(vertex_position_buffer);
    vgd->destroy_buffer(vertex_uv_buffer);
    vgd->destroy_buffer(index_buffer);
}