#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <stack>
#include <filesystem>
#include <thread>
#include "volk.h"
#include "vma.h"
#include "slotmap.h"
#include "stb_image.h"
#include "timer.h"
#include "VulkanGraphicsPipeline.h"

#define FRAMES_IN_FLIGHT 2		//Number of simultaneous frames the GPU could be working on
#define PIPELINE_CACHE_FILENAME ".shadercache"

constexpr VkComponentMapping COMPONENT_MAPPING_DEFAULT = {
	.r = VK_COMPONENT_SWIZZLE_R,
	.g = VK_COMPONENT_SWIZZLE_G,
	.b = VK_COMPONENT_SWIZZLE_B,
	.a = VK_COMPONENT_SWIZZLE_A,
};

struct VulkanBuffer {
	VkBuffer buffer;
	VmaAllocation allocation;
};

struct VulkanImage {
	uint32_t x;
	uint32_t y;
	uint32_t z;
	uint32_t mip_levels;
	VkImage image;
	VkImageView image_view;
	VmaAllocation image_allocation;
};

struct VulkanPendingImage {
	uint64_t image_upload_batch_id;
	VulkanImage vk_image;
};

struct VulkanAvailableImage {
	VulkanImage vk_image;
};

struct VulkanImageUploadBatch {
	uint64_t upload_id;
	uint64_t staging_buffer_id;
	VkCommandBuffer command_buffer;
};

struct VulkanGraphicsDevice {
	const VkAllocationCallbacks* alloc_callbacks;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkPhysicalDeviceFeatures2 device_features;
	VkDevice device;
	VkPipelineCache pipeline_cache;

	//Queue family indices
	uint32_t graphics_queue_family_idx;
	uint32_t compute_queue_family_idx;
	uint32_t transfer_queue_family_idx;

	VkCommandPool graphics_command_pool;
	VkCommandPool transfer_command_pool;
	VkCommandBuffer command_buffers[FRAMES_IN_FLIGHT];
	
	//State related to image uploading system
	std::vector<std::thread> image_upload_threads;
	VkSemaphore image_upload_semaphore;			//Timeline semaphore whose value increments by one for each image upload batch
	uint64_t image_uploads_requested = 0;
	uint64_t image_uploads_completed = 0;

	//These fields can be members of the graphics device struct because
	//we are assuming bindless resource management
	VkDescriptorSetLayout descriptor_set_layout;
	VkPipelineLayout pipeline_layout;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet descriptor_set;

	VmaAllocator allocator;		//Thank you, AMD
	const VmaDeviceMemoryCallbacks* vma_alloc_callbacks;

	VkCommandBuffer borrow_transfer_command_buffer();
	void return_transfer_command_buffer(VkCommandBuffer cb);

	void create_graphics_pipelines(
		uint64_t render_pass_handle,
		const char** shaderfiles,
		VulkanInputAssemblyState* ia_state,
		VulkanTesselationState* tess_state,
		VulkanViewportState* vp_state,
		VulkanRasterizationState* raster_state,
		VulkanMultisampleState* ms_state,
		VulkanDepthStencilState* ds_state,
		VulkanColorBlendState* blend_state,
		uint64_t* out_pipelines_handles,
		uint32_t pipeline_count
	);
	uint64_t load_images(
		uint32_t image_count,
		const std::vector<const char*> filenames,
		const std::vector<VkFormat> image_formats
	);
	uint64_t tick_image_uploads(VkCommandBuffer render_cb, std::vector<VkSemaphore>& wait_semaphores, std::vector<uint64_t>& wait_semaphore_values);

	VkSemaphore create_timeline_semaphore(uint64_t initial_value);
	uint64_t check_timeline_semaphore(VkSemaphore semaphore);

	uint64_t create_render_pass(VkRenderPassCreateInfo& info);

	VkRenderPass* get_render_pass(uint64_t key);
	VulkanGraphicsPipeline* get_graphics_pipeline(uint64_t key);

	VkShaderModule load_shader_module(const char* path);

	VulkanGraphicsDevice();
	~VulkanGraphicsDevice();

private:
	uint64_t load_images_impl(
		uint32_t image_count,
		const std::vector<const char*> filenames,
		const std::vector<VkFormat> image_formats
	);

	slotmap<VulkanBuffer> _buffers;

	slotmap<VulkanAvailableImage> _available_images;
	slotmap<VulkanPendingImage> _pending_images;
	slotmap<VulkanImageUploadBatch> _image_upload_batches;

	slotmap<VkRenderPass> _render_passes;
	slotmap<VulkanGraphicsPipeline> _graphics_pipelines;
	std::vector<VkSampler> _immutable_samplers;
	std::stack<VkCommandBuffer, std::vector<VkCommandBuffer>> _transfer_command_buffers;
};