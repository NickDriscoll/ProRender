#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <stack>
#include <filesystem>
#include "volk.h"
#include "vma.h"
#include "slotmap.h"
#include "VulkanGraphicsPipeline.h"
#define FRAMES_IN_FLIGHT 2		//Number of simultaneous frames the GPU could be working on
#define PIPELINE_CACHE_FILENAME ".shadercache"

constexpr VkComponentMapping COMPONENT_MAPPING_DEFAULT = {
	.r = VK_COMPONENT_SWIZZLE_R,
	.g = VK_COMPONENT_SWIZZLE_G,
	.b = VK_COMPONENT_SWIZZLE_B,
	.a = VK_COMPONENT_SWIZZLE_A,
};

struct VulkanGraphicsDevice {
	const VkAllocationCallbacks* alloc_callbacks;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkPhysicalDeviceFeatures2 device_features;
	VkDevice device;

	uint32_t graphics_queue_family_idx;
	uint32_t compute_queue_family_idx;
	uint32_t transfer_queue_family_idx;

	VkCommandPool graphics_command_pool;
	VkCommandPool transfer_command_pool;
	VkCommandBuffer command_buffers[FRAMES_IN_FLIGHT];
	VkPipelineCache pipeline_cache;
	
	VkSemaphore image_upload_semaphore;			//Timeline semaphore
	uint64_t image_upload_requests = 0;

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
	VulkanGraphicsPipeline* get_graphics_pipeline(uint64_t key);

	uint64_t create_render_pass(VkRenderPassCreateInfo& info);
	VkRenderPass* get_render_pass(uint64_t key);

	VkShaderModule load_shader_module(const char* path);

	VulkanGraphicsDevice();
	~VulkanGraphicsDevice();

private:
	slotmap<VkRenderPass> _render_passes;
	slotmap<VulkanGraphicsPipeline> _graphics_pipelines;
	std::vector<VkSampler> _immutable_samplers;
	std::stack<VkCommandBuffer, std::vector<VkCommandBuffer>> _transfer_command_buffers;
};