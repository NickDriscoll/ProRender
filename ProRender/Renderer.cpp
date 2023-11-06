#include "Renderer.h"

Renderer::Renderer(VulkanGraphicsDevice* vgd) {
    
    //Allocate memory for ImGUI vertex data
    //TODO: probabaly shouldn't be in renderer init
    {
        VkDeviceSize buffer_size = 512 * 1024;

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        alloc_info.priority = 1.0;

        imgui_vertex_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, alloc_info);
        imgui_index_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, alloc_info);
    }

    //Save pointer to graphics device
    this->vgd = vgd;
}

Renderer::~Renderer() {
    vgd->destroy_buffer(imgui_vertex_buffer);
    vgd->destroy_buffer(imgui_index_buffer);
}