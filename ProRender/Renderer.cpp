#include "Renderer.h"

Renderer::Renderer(VulkanGraphicsDevice* vgd) {
    
    //Allocate memory for ImGUI vertex data
    //TODO: probabaly shouldn't be in renderer init
    {
        VkDeviceSize buffer_size = 256 * 1024;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        alloc_info.priority = 1.0;

        imgui_vertex_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
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
        
        {
            VkDescriptorBufferInfo buffer_info = {
                .buffer = vgd->get_buffer(frame_uniforms_buffer)->buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            };
            
            VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vgd->descriptor_set,
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_info
            };
            descriptor_writes.push_back(write);
        }

        {
            VkDescriptorBufferInfo buffer_info = {
                .buffer = vgd->get_buffer(imgui_vertex_buffer)->buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            };
            
            VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = vgd->descriptor_set,
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &buffer_info
            };
            descriptor_writes.push_back(write);
        }        

        vkUpdateDescriptorSets(vgd->device, static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data(), 0, nullptr);
	}

    //Save pointer to graphics device
    this->vgd = vgd;
}

Renderer::Renderer(VulkanGraphicsDevice* vgd, uint32_t width, uint32_t height) {
    *this = Renderer::Renderer(vgd);
    frame_uniforms.clip_from_screen = hlslpp::float4x4(
        2.0 / width, 0.0, 0.0, 0.0,
        0.0, 2.0 / height, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );
}

Renderer::~Renderer() {
    vgd->destroy_buffer(imgui_vertex_buffer);
    vgd->destroy_buffer(imgui_index_buffer);
    vgd->destroy_buffer(frame_uniforms_buffer);
}