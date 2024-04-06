#include "Renderer.h"
#include "imgui.h"

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

Renderer::Renderer(VulkanGraphicsDevice* vgd, Key<VkRenderPass> swapchain_renderpass) {

    cameras.alloc(MAX_CAMERAS);
    _position_buffers.alloc(MAX_VERTEX_ATTRIBS);
    _uv_buffers.alloc(MAX_VERTEX_ATTRIBS);
    _index16_buffers.alloc(MAX_VERTEX_ATTRIBS);

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
	graphics_timeline_semaphore = vgd->create_timeline_semaphore(0);

    //Save pointer to graphics device
    this->vgd = vgd;
}

Key<BufferView> Renderer::push_vertex_positions(std::span<float> data) {
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

BufferView* Renderer::get_vertex_positions(Key<BufferView> key) {
    return _position_buffers.get(key);
}

Key<MeshAttribute> Renderer::push_vertex_uvs(Key<BufferView> position_key, std::span<float> data) {
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
BufferView* Renderer::get_vertex_uvs(Key<BufferView> position_key) {

    BufferView* result = nullptr;
    for (MeshAttribute& att : _uv_buffers) {
        if (att.position_key.value() == position_key.value()) {
            result = &att.view;
            break;
        }
    }

    return result;
}

Key<MeshAttribute> Renderer::push_indices16(Key<BufferView> position_key, std::span<uint16_t> data) {
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

BufferView* Renderer::get_indices16(Key<BufferView> position_key) {

    BufferView* result = nullptr;
    for (MeshAttribute& att : _index16_buffers) {
        if (att.position_key.value() == position_key.value()) {
            result = &att.view;
            break;
        }
    }

    return result;

}

Renderer::~Renderer() {
	for (uint32_t i = 0; i < _samplers.size(); i++) {
		vkDestroySampler(vgd->device, _samplers[i], vgd->alloc_callbacks);
	}

	vkDestroySemaphore(vgd->device, graphics_timeline_semaphore, vgd->alloc_callbacks);

	vkDestroyDescriptorPool(vgd->device, descriptor_pool, vgd->alloc_callbacks);
    vgd->destroy_buffer(camera_buffer);
    vgd->destroy_buffer(frame_uniforms_buffer);
    vgd->destroy_buffer(vertex_position_buffer);
    vgd->destroy_buffer(vertex_uv_buffer);
    vgd->destroy_buffer(index_buffer);
}