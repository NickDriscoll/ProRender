#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <stack>
#include <deque>
#include <queue>
#include <filesystem>
#include <thread>
#include <mutex>
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
	VmaAllocationInfo alloc_info;
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

struct RawImage {
	uint32_t width;
	uint32_t height;
	uint8_t* data;
};

struct VulkanPendingImage {
	uint64_t batch_id;
	VulkanImage vk_image;
};

struct VulkanAvailableImage {
	uint64_t batch_id;
	VulkanImage vk_image;
};

struct VulkanImageUploadBatch {
	uint64_t id;
	uint64_t staging_buffer_id;
	VkCommandBuffer command_buffer;
};

struct RawImageBatchParameters {
	const uint64_t id;
	const std::vector<RawImage> raw_images;
	const std::vector<VkFormat> image_formats;
};

struct FileImageBatchParameters {
	const uint64_t id;
	const std::vector<const char*> filenames;
	const std::vector<VkFormat> image_formats;
};

struct VulkanGraphicsDevice {
	const VkAllocationCallbacks* alloc_callbacks;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkPhysicalDeviceFeatures2 device_features;
	VkPhysicalDeviceLimits physical_limits;
	VkDevice device;
	VkPipelineCache pipeline_cache;

	//Queue family indices
	uint32_t graphics_queue_family_idx;
	uint32_t compute_queue_family_idx;
	uint32_t transfer_queue_family_idx;

	VkCommandPool graphics_command_pool;
	VkCommandPool transfer_command_pool;
	VkCommandBuffer command_buffers[FRAMES_IN_FLIGHT];
	VkSemaphore image_upload_semaphore;			//Timeline semaphore whose value increments by one for each image upload batch
	
	slotmap<VulkanAvailableImage> available_images;

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

	uint64_t create_buffer(VkDeviceSize size, VkBufferUsageFlags usage_flags, VmaAllocationCreateInfo& allocation_info);
	VulkanBuffer* get_buffer(uint64_t key);
	void destroy_buffer(uint64_t key);

	uint64_t load_raw_images(
		const std::vector<RawImage> raw_images,
		const std::vector<VkFormat> image_formats
	);
	uint64_t load_image_files(
		const std::vector<const char*> filenames,
		const std::vector<VkFormat> image_formats
	);
	void tick_image_uploads(VkCommandBuffer render_cb);
	uint64_t get_completed_image_uploads();

	VkSemaphore create_timeline_semaphore(uint64_t initial_value);
	uint64_t check_timeline_semaphore(VkSemaphore semaphore);

	uint64_t create_render_pass(VkRenderPassCreateInfo& info);

	VkRenderPass* get_render_pass(uint64_t key);
	VulkanGraphicsPipeline* get_graphics_pipeline(uint64_t key);

	VkShaderModule load_shader_module(const char* path);

	VulkanGraphicsDevice();
	~VulkanGraphicsDevice();

private:
	void load_images_impl();
	void submit_image_upload_batch(uint64_t id, const std::vector<RawImage>& raw_images, const std::vector<VkFormat>& image_formats);

	slotmap<VulkanBuffer> _buffers;

	
	//State related to image uploading system
	bool _image_upload_running = true;
	uint64_t _image_uploads_requested = 0;
	uint64_t _image_uploads_completed = 0;
	std::queue<RawImageBatchParameters, std::deque<RawImageBatchParameters>> _raw_image_batch_queue;
	std::mutex _raw_image_mutex;
	std::queue<FileImageBatchParameters, std::deque<FileImageBatchParameters>> _image_file_batch_queue;
	std::mutex _file_batch_mutex;
	std::thread _image_upload_thread;
	slotmap<VulkanPendingImage> _pending_images;
	std::mutex _pending_image_mutex;
	slotmap<VulkanImageUploadBatch> _image_upload_batches;
	std::mutex _image_upload_mutex;


	slotmap<VkRenderPass> _render_passes;
	slotmap<VulkanGraphicsPipeline> _graphics_pipelines;
	std::vector<VkSampler> _immutable_samplers;
	std::stack<VkCommandBuffer, std::vector<VkCommandBuffer>> _graphics_command_buffers;
	std::stack<VkCommandBuffer, std::vector<VkCommandBuffer>> _transfer_command_buffers;
};
