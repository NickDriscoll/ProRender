#include <filesystem>
#include <stdio.h>
#include "VulkanGraphicsDevice.h"

VulkanGraphicsDevice::VulkanGraphicsDevice() {
	alloc_callbacks = nullptr;			//TODO: Custom allocator
	vma_alloc_callbacks = nullptr;

	//Initialize volk
	if (volkInitialize() != VK_SUCCESS) {
		printf("RIP\n");
		exit(-1);
	}
	printf("Volk initialized.\n");

	//Vulkan instance creation
	uint32_t VK_API_VERSION = VK_MAKE_API_VERSION(0, 1, 2, 0);
	{
		VkApplicationInfo app_info;
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pNext = nullptr;
		app_info.pApplicationName = "ProRender";
		app_info.applicationVersion = 1;
		app_info.pEngineName = nullptr;
		app_info.engineVersion = 0;
		app_info.apiVersion = VK_API_VERSION;

		//Instance extension configuration
		std::vector<const char*> extension_names;

		extension_names.push_back(static_cast<const char*>("VK_KHR_surface"));

		//Platform specific surface extensions
#ifdef _WIN32
		extension_names.push_back(static_cast<const char*>("VK_KHR_win32_surface"));
#endif

#ifdef __linux__
		extension_names.push_back(static_cast<const char*>("VK_KHR_xlib_surface"));
#endif

		VkInstanceCreateInfo inst_info;
		inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		inst_info.pNext = nullptr;
		inst_info.flags = 0;
		inst_info.pApplicationInfo = &app_info;
		inst_info.enabledLayerCount = 0;
		inst_info.ppEnabledLayerNames = nullptr;
		inst_info.enabledExtensionCount = extension_names.size();
		inst_info.ppEnabledExtensionNames = extension_names.data();

		if (vkCreateInstance(&inst_info, alloc_callbacks, &instance) != VK_SUCCESS) {
			printf("Instance creation failed.\n");
			exit(-1);
		}
		printf("Instance created.\n");
	}

	//Load all Vulkan instance functions
	volkLoadInstanceOnly(instance);

	//Vulkan physical device selection
	VkPhysicalDeviceTimelineSemaphoreFeatures semaphore_features = {};
	VkPhysicalDeviceSynchronization2Features sync2_features = {};
	VkPhysicalDeviceDescriptorIndexingFeatures desciptor_indexing_features = {};
	{
		uint32_t physical_device_count = 0;
		//Getting physical device count by passing nullptr as last param
		if (vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr) != VK_SUCCESS) {
			printf("Querying physical device count failed.\n");
			exit(-1);
		}
		printf("%i physical devices available.\n", physical_device_count);

		std::vector<VkPhysicalDevice> devices;
		devices.resize(physical_device_count);
		if (vkEnumeratePhysicalDevices(instance, &physical_device_count, devices.data()) != VK_SUCCESS) {
			printf("Querying physical devices failed.\n");
			exit(-1);
		}

		const VkPhysicalDeviceType TYPES[] = { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, VK_PHYSICAL_DEVICE_TYPE_CPU };
		for (uint32_t j = 0; j < 3; j++) {
			for (uint32_t i = 0; i < physical_device_count; i++) {
				VkPhysicalDevice device = devices[i];

				VkPhysicalDeviceProperties2 device_properties = {};
				device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				vkGetPhysicalDeviceProperties2(device, &device_properties);

				uint32_t queue_count = 0;
				vkGetPhysicalDeviceQueueFamilyProperties2(device, &queue_count, nullptr);

				std::vector<VkQueueFamilyProperties2> queue_properties;
				queue_properties.resize(queue_count);
				//C++ initialization is hell
				for (uint32_t k = 0; k < queue_count; k++) {
					queue_properties[k].pNext = nullptr;
					queue_properties[k].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
				}

				vkGetPhysicalDeviceQueueFamilyProperties2(device, &queue_count, queue_properties.data());

				//Check for compute and transfer queues
				for (uint32_t k = 0; k < queue_count; k++) {
					VkQueueFamilyProperties2& props = queue_properties[k];
					if (props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
						graphics_queue_family_idx = k;
						compute_queue_family_idx = k;
						transfer_queue_family_idx = k;
						continue;
					}

					if ((props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
						props.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
						compute_queue_family_idx = k;
						continue;
					}

					if ((props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
						props.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) {
						transfer_queue_family_idx = k;
						continue;
					}
				}

				if (device_properties.properties.deviceType == TYPES[j]) {
					physical_device = device;
					printf("Chosen physical device: %s\n", device_properties.properties.deviceName);
					break;
				}
			}

			if (physical_device) {
				//Physical device feature checking section
				semaphore_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
				sync2_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
				device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				desciptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

				sync2_features.pNext = &desciptor_indexing_features;
				semaphore_features.pNext = &sync2_features;
				device_features.pNext = &semaphore_features;
				vkGetPhysicalDeviceFeatures2(physical_device, &device_features);

				if (!desciptor_indexing_features.runtimeDescriptorArray) {
					printf("No support for descriptor arrays on this device.\n");
					exit(-1);
				}

				if (!semaphore_features.timelineSemaphore) {
					printf("No support for timeline semaphores on this device.\n");
					exit(-1);
				}

				if (!sync2_features.synchronization2) {
					printf("No support for sync2 on this device.\n");
					exit(-1);
				}
				break;
			};
		}
	}

	//Vulkan device creation
	{
		std::vector<VkDeviceQueueCreateInfo> queue_infos;
		queue_infos.reserve(3);

		float priority = 1.0;	//Static priority value because of course

		//We always have a general GRAPHICS queue family
		VkDeviceQueueCreateInfo g_queue_info = {};
		g_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		g_queue_info.queueFamilyIndex = graphics_queue_family_idx;
		g_queue_info.queueCount = 1;
		g_queue_info.pQueuePriorities = &priority;
		queue_infos.push_back(g_queue_info);

		//If we have a dedicated compute queue family
		if (graphics_queue_family_idx != compute_queue_family_idx) {
			VkDeviceQueueCreateInfo c_queue_info = {};
			c_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			c_queue_info.queueFamilyIndex = compute_queue_family_idx;
			c_queue_info.queueCount = 1;
			c_queue_info.pQueuePriorities = &priority;
			queue_infos.push_back(c_queue_info);
		}

		//If we have a dedicated transfer queue family
		if (graphics_queue_family_idx != transfer_queue_family_idx) {
			VkDeviceQueueCreateInfo t_queue_info = {};
			t_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			t_queue_info.queueFamilyIndex = transfer_queue_family_idx;
			t_queue_info.queueCount = 1;
			t_queue_info.pQueuePriorities = &priority;
			queue_infos.push_back(t_queue_info);
		}

		std::vector<const char*> extension_names = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
		};

		VkDeviceCreateInfo device_info = {};
		device_info.pNext = &device_features;
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.flags = 0;
		device_info.queueCreateInfoCount = queue_infos.size();
		device_info.pQueueCreateInfos = queue_infos.data();
		device_info.enabledLayerCount = 0;
		device_info.ppEnabledLayerNames = nullptr;
		device_info.enabledExtensionCount = extension_names.size();
		device_info.ppEnabledExtensionNames = extension_names.data();
		device_info.pEnabledFeatures = nullptr;

		if (vkCreateDevice(physical_device, &device_info, alloc_callbacks, &device) != VK_SUCCESS) {
			printf("Creating logical device failed.\n");
			exit(-1);
		}
	}
	printf("Logical device created.\n");

	//Load all device functions
	volkLoadDevice(device);

	//Initialize VMA
	{
		VmaAllocatorCreateInfo info = {};
		info.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
		info.instance = instance;
		info.physicalDevice = physical_device;
		info.device = device;
		info.pAllocationCallbacks = alloc_callbacks;
		info.pDeviceMemoryCallbacks = vma_alloc_callbacks;
		info.vulkanApiVersion = VK_API_VERSION;

		VmaVulkanFunctions vkfns = {};
		vkfns.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vkfns.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
		info.pVulkanFunctions = &vkfns;

		if (vmaCreateAllocator(&info, &allocator) != VK_SUCCESS) {
			printf("Creating memory allocator failed.\n");
			exit(-1);
		}
	}
	printf("VMA memory allocator created.\n");

	//Create command pool
	{
		VkCommandPoolCreateInfo pool_info;
		pool_info.pNext = nullptr;
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pool_info.queueFamilyIndex = graphics_queue_family_idx;

		if (vkCreateCommandPool(device, &pool_info, alloc_callbacks, &graphics_command_pool) != VK_SUCCESS) {
			printf("Creating main command pool failed.\n");
			exit(-1);
		}
	}
	printf("Command pool created.\n");

	//Create command pool
	{
		VkCommandPoolCreateInfo pool_info;
		pool_info.pNext = nullptr;
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pool_info.queueFamilyIndex = transfer_queue_family_idx;

		if (vkCreateCommandPool(device, &pool_info, alloc_callbacks, &transfer_command_pool) != VK_SUCCESS) {
			printf("Creating main command pool failed.\n");
			exit(-1);
		}
	}
	printf("Command pool created.\n");

	{
		std::vector<VkCommandBuffer> storage;
		storage.resize(128);

		VkCommandBufferAllocateInfo cb_info = {};
		cb_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cb_info.commandPool = transfer_command_pool;
		cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cb_info.commandBufferCount = storage.size();

		if (vkAllocateCommandBuffers(device, &cb_info, storage.data()) != VK_SUCCESS) {
			printf("Creating main command buffers failed.\n");
			exit(-1);
		}

		_transfer_command_buffers = std::stack<VkCommandBuffer, std::vector<VkCommandBuffer>>(storage);
	}

	//Allocate command buffers
	{
		VkCommandBufferAllocateInfo cb_info = {};
		cb_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cb_info.commandPool = graphics_command_pool;
		cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cb_info.commandBufferCount = FRAMES_IN_FLIGHT;

		if (vkAllocateCommandBuffers(device, &cb_info, command_buffers) != VK_SUCCESS) {
			printf("Creating main command buffers failed.\n");
			exit(-1);
		}
	}
	printf("Command buffers allocated.\n");

	{
		VkSemaphoreTypeCreateInfo type_info = {};
		type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		type_info.initialValue = 0;

		VkSemaphoreCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		info.pNext = &type_info;

		if (vkCreateSemaphore(device, &info, alloc_callbacks, &image_upload_semaphore) != VK_SUCCESS) {
			printf("Creating image upload timeline semaphore failed.\n");
			exit(-1);
		}
	}

	//Create pipeline cache
	{
		VkPipelineCacheCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

		std::vector<uint8_t> cache_data;
		if (std::filesystem::exists(PIPELINE_CACHE_FILENAME)) {
			printf("Found pipeline cache.\n");
			uint32_t pipeline_size = std::filesystem::file_size(PIPELINE_CACHE_FILENAME);
			cache_data.resize(pipeline_size);
			FILE* f = fopen(PIPELINE_CACHE_FILENAME, "rb");
			if (fread(cache_data.data(), sizeof(uint8_t), pipeline_size, f) == 0) {
				printf("Error reading pipeline cache.\n");
				exit(-1);
			}
			fclose(f);

			info.initialDataSize = pipeline_size;
			info.pInitialData = cache_data.data();
		}

		if (vkCreatePipelineCache(device, &info, alloc_callbacks, &pipeline_cache) != VK_SUCCESS) {
			printf("Creating pipeline cache failed.\n");
			exit(-1);
		}
	}
	printf("Created pipeline cache.\n");

	//Pipeline layout
	{
		//Immutable sampler creation
		VkSampler sampler;
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
				.maxAnisotropy = 16.0
			};

			if (vkCreateSampler(device, &info, alloc_callbacks, &sampler) != VK_SUCCESS) {
				printf("Creating sampler failed.\n");
				exit(-1);
			}
		}

		//Descriptor set layout
		{
			std::vector<VkDescriptorSetLayoutBinding> bindings;

			VkDescriptorSetLayoutBinding sampled_image_binding = {
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			};
			bindings.push_back(sampled_image_binding);


			VkDescriptorSetLayoutBinding sampler_binding = {
				.binding = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = &sampler
			};
			bindings.push_back(sampler_binding);

			VkDescriptorSetLayoutCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			info.bindingCount = bindings.size();
			info.pBindings = bindings.data();

			if (vkCreateDescriptorSetLayout(device, &info, alloc_callbacks, &descriptor_set_layout) != VK_SUCCESS) {
				printf("Creating descriptor set layout failed.\n");
				exit(-1);
			}
			//vkDestroySampler(device, sampler, alloc_callbacks);
		}

		VkPushConstantRange range;
		range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		range.offset = 0;
		range.size = 8;

		VkPipelineLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.setLayoutCount = 1;
		info.pSetLayouts = &descriptor_set_layout;
		info.pushConstantRangeCount = 1;
		info.pPushConstantRanges = &range;


		if (vkCreatePipelineLayout(device, &info, alloc_callbacks, &pipeline_layout) != VK_SUCCESS) {
			printf("Creating pipeline layout failed.\n");
			exit(-1);
		}
	}
	printf("Created pipeline layout.\n");

	//Create bindless descriptor set
	{
		{
			VkDescriptorPoolSize sizes[] = {
				{
					.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
					.descriptorCount = 1,
				},
				{
					.type = VK_DESCRIPTOR_TYPE_SAMPLER,
					.descriptorCount = 1
				}
			};

			VkDescriptorPoolCreateInfo info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
				.maxSets = 1,
				.poolSizeCount = 2,
				.pPoolSizes = sizes
			};

			if (vkCreateDescriptorPool(device, &info, alloc_callbacks, &descriptor_pool) != VK_SUCCESS) {
				printf("Creating descriptor pool failed.\n");
				exit(-1);
			}
		}

		{
			VkDescriptorSetAllocateInfo info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
				.descriptorPool = descriptor_pool,
				.descriptorSetCount = 1,
				.pSetLayouts = &descriptor_set_layout
			};

			if (vkAllocateDescriptorSets(device, &info, &descriptor_set) != VK_SUCCESS) {
				printf("Allocating descriptor set failed.\n");
				exit(-1);
			}
		}
	}
}

VulkanGraphicsDevice::~VulkanGraphicsDevice() {
	//Write pipeline cache to file
	{
		size_t data_size;
		if (vkGetPipelineCacheData(device, pipeline_cache, &data_size, nullptr) != VK_SUCCESS) {
			printf("Pipeline cache size query failed.\n");
			exit(-1);
		}

		std::vector<uint8_t> cache_data;
		cache_data.resize(data_size);
		if (vkGetPipelineCacheData(device, pipeline_cache, &data_size, cache_data.data()) != VK_SUCCESS) {
			printf("Pipeline cache data query failed.\n");
			exit(-1);
		}

		FILE* f = fopen(PIPELINE_CACHE_FILENAME, "wb");
		if (fwrite(cache_data.data(), sizeof(uint8_t), data_size, f) == 0) {
			printf("Error writing pipeline cache data.\n");
			exit(-1);
		}
		fclose(f);
	}
	
	
	vkDestroySemaphore(device, image_upload_semaphore, alloc_callbacks);

	vkDestroyPipelineLayout(device, pipeline_layout, alloc_callbacks);
	vkDestroyDescriptorSetLayout(device, descriptor_set_layout, alloc_callbacks);
	vkDestroyCommandPool(device, transfer_command_pool, alloc_callbacks);
	vkDestroyCommandPool(device, graphics_command_pool, alloc_callbacks);
	vkDestroyPipelineCache(device, pipeline_cache, alloc_callbacks);

	vmaDestroyAllocator(allocator);

	vkDestroyDevice(device, alloc_callbacks);
	vkDestroyInstance(instance, alloc_callbacks);
}

VkCommandBuffer VulkanGraphicsDevice::borrow_transfer_command_buffer() {
	VkCommandBuffer cb = _transfer_command_buffers.top();
	_transfer_command_buffers.pop();
	return cb;
}

void VulkanGraphicsDevice::return_command_buffer(VkCommandBuffer cb) {
	_transfer_command_buffers.push(cb);
}

VkShaderModule VulkanGraphicsDevice::load_shader_module(const char* path) {
	//Get filesize
	uint32_t spv_size = std::filesystem::file_size(std::filesystem::path(path));

	//Allocate receiving buffer
	std::vector<uint32_t> spv_bytes;
	spv_bytes.resize(spv_size);

	//Read spv bytes
	FILE* shader_spv = fopen(path, "rb");
	if (fread(spv_bytes.data(), sizeof(uint8_t), spv_size, shader_spv) == 0) {
		printf("Zero bytes read when trying to read %s\n", path);
		exit(-1);
	}
	fclose(shader_spv);

	//Create shader module
	VkShaderModule shader;
	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = spv_size;
	info.pCode = spv_bytes.data();
	if (vkCreateShaderModule(device, &info, alloc_callbacks, &shader) != VK_SUCCESS) {
		printf("Creating vertex shader module failed.\n");
		exit(-1);
	}

	return shader;
}