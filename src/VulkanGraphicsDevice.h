#pragma once

#include <deque>
#include <queue>
#include <mutex>
#include "volk.h"
#include "vma.h"
#include "slotmap.h"
#include "VulkanGraphicsPipeline.h"

#define FRAMES_IN_FLIGHT 2		//Number of simultaneous frames the GPU could be working on
#define PIPELINE_CACHE_FILENAME ".shadercache"

enum DescriptorBindings : uint8_t {
	SAMPLED_IMAGES,
	SAMPLERS
};

enum ImmutableSamplers : uint8_t {
	STANDARD,
	NEAREST,
	FULL_ANISO = STANDARD
};

constexpr VkComponentMapping COMPONENT_MAPPING_DEFAULT = {
	.r = VK_COMPONENT_SWIZZLE_R,
	.g = VK_COMPONENT_SWIZZLE_G,
	.b = VK_COMPONENT_SWIZZLE_B,
	.a = VK_COMPONENT_SWIZZLE_A,
};

struct VulkanDescriptorLayoutBinding {
	VkDescriptorType descriptor_type;
	uint32_t descriptor_count;
	VkShaderStageFlags stage_flags;
	const VkSampler* immutable_samplers;
};

struct VulkanBuffer {
	VkBuffer buffer;
	VmaAllocation allocation;
	VmaAllocationInfo alloc_info;
};

struct VulkanImage {
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t mip_levels;
	VkImage image;
	VkImageView image_view;
	VmaAllocation image_allocation;
};

struct VulkanFrameBuffer {
	uint32_t width;
	uint32_t height;
	Key<VkFramebuffer> fb;
	Key<VkRenderPass> render_pass;
};

struct BufferDeletion {
	uint32_t idx;
	uint32_t frames_til;
	VkBuffer buffer;
	VmaAllocation allocation;
};

struct ImageDeletion {
	uint32_t idx;
	uint32_t frames_til;
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
	uint32_t original_idx;
	VulkanImage vk_image;
};

struct VulkanAvailableImage {
	uint64_t batch_id;
	uint32_t original_idx;
	VulkanImage vk_image;
};

struct VulkanImageUploadBatch {
	uint64_t id;
	Key<VulkanBuffer> staging_buffer_id;
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

struct SemaphoreWait {
	uint64_t wait_value;
	Key<VkSemaphore> wait_semaphore;
};

struct CommandBufferReturn {
	VkCommandBuffer cb;
	uint64_t wait_value;	//Could be zero if the semaphore is binary
	Key<VkSemaphore> wait_semaphore;
};

struct DescriptorLayout {
	
};

struct SyncData {
	std::vector<uint64_t> wait_values;
	std::vector<uint64_t> signal_values;
	std::vector<VkSemaphore> wait_semaphores;
	std::vector<VkSemaphore> signal_semaphores;
	VkFence fence = VK_NULL_HANDLE;
	VkSemaphore cpu_wait_semaphore;
	uint64_t cpu_wait_value;
};

struct VulkanGraphicsDevice {
	const VkAllocationCallbacks* alloc_callbacks;
	VkInstance instance;
	VkPhysicalDevice physical_device = 0;
	VkPhysicalDeviceFeatures2 device_features;
	VkPhysicalDeviceLimits physical_limits;
	VkDevice device = 0;
	VkPipelineCache pipeline_cache;

	//Queue family indices
	uint32_t graphics_queue_family_idx;
	uint32_t compute_queue_family_idx;
	uint32_t transfer_queue_family_idx;

	VkCommandPool graphics_command_pool;
	VkCommandPool transfer_command_pool;
	Key<VkSemaphore> image_upload_semaphore;			//Timeline semaphore whose value increments by one for each image upload batch
	
	slotmap<VulkanAvailableImage> available_images;

	VmaAllocator allocator;		//Thank you, AMD
	const VmaDeviceMemoryCallbacks* vma_alloc_callbacks;

	VkCommandBuffer get_graphics_command_buffer();
	void return_command_buffer(VkCommandBuffer cb, uint64_t wait_value, Key<VkSemaphore> wait_semaphore);
	VkCommandBuffer borrow_transfer_command_buffer();
	void return_transfer_command_buffer(VkCommandBuffer cb);

	void create_graphics_pipelines(
		const std::vector<VulkanGraphicsPipelineConfig>& pipeline_configs,
		Key<VulkanGraphicsPipeline>* out_pipelines_handles
	);
	VulkanGraphicsPipeline* get_graphics_pipeline(Key<VulkanGraphicsPipeline> key);

	Key<VulkanBuffer> create_buffer(VkDeviceSize size, VkBufferUsageFlags usage_flags, VmaAllocationCreateInfo& allocation_info);
	VulkanBuffer* get_buffer(Key<VulkanBuffer> key);
	VkDeviceAddress buffer_device_address(Key<VulkanBuffer> key);
	void destroy_buffer(Key<VulkanBuffer> key);

	Key<VkSemaphore> create_semaphore(VkSemaphoreCreateInfo& info);
	VkSemaphore* get_semaphore(Key<VkSemaphore> key);
	Key<VkSemaphore> create_timeline_semaphore(uint64_t initial_value);
	uint64_t check_timeline_semaphore(Key<VkSemaphore> semaphore);

	VkSampler create_sampler(VkSamplerCreateInfo& info);

	//Image uploading system
	uint64_t load_raw_images(
		const std::vector<RawImage> raw_images,
		const std::vector<VkFormat> image_formats
	);
	uint64_t load_image_files(
		const std::vector<const char*> filenames,
		const std::vector<VkFormat> image_formats
	);
	void tick_image_uploads(VkCommandBuffer render_cb);
	uint64_t completed_image_batches();
	void destroy_image(Key<VulkanAvailableImage> key);
	VkPipelineLayout get_pipeline_layout();

	Key<VkFramebuffer> create_framebuffer(VkFramebufferCreateInfo& info);
	VkFramebuffer* get_framebuffer(Key<VkFramebuffer> key);
	void destroy_framebuffer(Key<VkFramebuffer> key);
	void begin_render_pass(VkCommandBuffer cb, VulkanFrameBuffer& fb);
	void end_render_pass(VkCommandBuffer cb);

	Key<VkRenderPass> create_render_pass(VkRenderPassCreateInfo& info);
	VkRenderPass* get_render_pass(Key<VkRenderPass> key);

	VkShaderModule load_shader_module(const char* path);

	void service_deletion_queues();
	
	void graphics_queue_submit(VkCommandBuffer cb, SyncData& sems);

	VulkanGraphicsDevice();
	~VulkanGraphicsDevice();

private:
	void load_images_impl();
	void submit_image_upload_batch(uint64_t id, const std::vector<RawImage>& raw_images, const std::vector<VkFormat>& image_formats);

	slotmap<VulkanBuffer> _buffers;
	std::deque<BufferDeletion> _buffer_deletion_queue;

	//State related to image uploading system
	bool _image_upload_running = true;
	uint64_t _image_batches_requested = 0;
	uint64_t _image_batches_completed = 0;
	std::queue<RawImageBatchParameters, std::deque<RawImageBatchParameters>> _raw_image_batch_queue;
	std::mutex _raw_image_mutex;
	std::queue<FileImageBatchParameters, std::deque<FileImageBatchParameters>> _image_file_batch_queue;
	std::mutex _file_batch_mutex;
	std::thread _image_upload_thread;
	slotmap<VulkanPendingImage> _pending_images;
	std::mutex _pending_image_mutex;
	slotmap<VulkanImageUploadBatch> _image_upload_batches;
	std::mutex _image_upload_mutex;
	std::deque<ImageDeletion> _image_deletion_queue;
	VkDescriptorPool _descriptor_pool;
	VkDescriptorSet _image_descriptor_set;
	VkDescriptorSetLayout _image_descriptor_set_layout;
	VkPipelineLayout _pipeline_layout;
	std::vector<VkSampler> _immutable_samplers;


	slotmap<VkFramebuffer> _framebuffers;
	slotmap<VkRenderPass> _render_passes;
	slotmap<VkSemaphore> _semaphores;
	slotmap<VulkanGraphicsPipeline> _graphics_pipelines;
	std::stack<VkCommandBuffer, std::vector<VkCommandBuffer>> _graphics_command_buffers;
	std::deque<CommandBufferReturn> _command_buffer_returns;
	std::stack<VkCommandBuffer, std::vector<VkCommandBuffer>> _transfer_command_buffers;
};
