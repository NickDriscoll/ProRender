#include <filesystem>
#include "VulkanGraphicsDevice.h"

void VulkanGraphicsDevice::init() {
	alloc_callbacks = nullptr;			//TODO: Custom allocator

	//Initialize volk
	if (volkInitialize() != VK_SUCCESS) {
		printf("RIP\n");
		exit(-1);
	}
	printf("Volk initialized.\n");

	//Vulkan instance creation
	{
		VkApplicationInfo app_info;
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pNext = nullptr;
		app_info.pApplicationName = "ProRender";
		app_info.applicationVersion = 1;
		app_info.pEngineName = nullptr;
		app_info.engineVersion = 0;
		app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0);

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

				device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
				device_features.pNext = &semaphore_features;
				vkGetPhysicalDeviceFeatures2(physical_device, &device_features);

				if (!semaphore_features.timelineSemaphore) {
					printf("No support for timeline semaphores on this device.\n");
					exit(-1);
				}
				break;
			};
		}
	}

	//Vulkan device creation
	{
		std::vector<VkDeviceQueueCreateInfo> queue_infos;

		float priority = 1.0;	//Static priority value because of course

		//We always have a general GRAPHICS queue family
		VkDeviceQueueCreateInfo g_queue_info;
		g_queue_info.pNext = nullptr;
		g_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		g_queue_info.flags = 0;
		g_queue_info.queueFamilyIndex = graphics_queue_family_idx;
		g_queue_info.queueCount = 1;
		g_queue_info.pQueuePriorities = &priority;
		queue_infos.push_back(g_queue_info);

		//If we have a dedicated compute queue family
		if (graphics_queue_family_idx != compute_queue_family_idx) {
			VkDeviceQueueCreateInfo c_queue_info;
			c_queue_info.pNext = nullptr;
			c_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			c_queue_info.flags = 0;
			c_queue_info.queueFamilyIndex = compute_queue_family_idx;
			c_queue_info.queueCount = 1;
			c_queue_info.pQueuePriorities = &priority;
			queue_infos.push_back(c_queue_info);
		}

		//If we have a dedicated transfer queue family
		if (graphics_queue_family_idx != transfer_queue_family_idx) {
			VkDeviceQueueCreateInfo t_queue_info;
			t_queue_info.pNext = nullptr;
			t_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			t_queue_info.flags = 0;
			t_queue_info.queueFamilyIndex = transfer_queue_family_idx;
			t_queue_info.queueCount = 1;
			t_queue_info.pQueuePriorities = &priority;
			queue_infos.push_back(t_queue_info);
		}

		std::vector<const char*> extension_names = {"VK_KHR_swapchain"};

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

	//Create command pool
	{
		VkCommandPoolCreateInfo pool_info;
		pool_info.pNext = nullptr;
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pool_info.queueFamilyIndex = graphics_queue_family_idx;

		if (vkCreateCommandPool(device, &pool_info, alloc_callbacks, &command_pool) != VK_SUCCESS) {
			printf("Creating main command pool failed.\n");
			exit(-1);
		}
	}
	printf("Command pool created.\n");

	//Allocate command buffers
	{
		VkCommandBufferAllocateInfo cb_info;
		cb_info.pNext = nullptr;
		cb_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cb_info.commandPool = command_pool;
		cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cb_info.commandBufferCount = FRAMES_IN_FLIGHT;

		if (vkAllocateCommandBuffers(device, &cb_info, command_buffers) != VK_SUCCESS) {
			printf("Creating main command buffers failed.\n");
			exit(-1);
		}
	}
	printf("Command buffers allocated.\n");

	//Create pipeline cache
	{
		VkPipelineCacheCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		info.initialDataSize = 0;
		if (vkCreatePipelineCache(device, &info, alloc_callbacks, &pipeline_cache) != VK_SUCCESS) {
			printf("Creating pipeline cache failed.\n");
			exit(-1);
		}
	}
	printf("Created pipeline cache.\n");

	//Pipeline layout
	{
		//Descriptor set layout
		{
			VkDescriptorSetLayoutCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;


			if (vkCreateDescriptorSetLayout(device, &info, alloc_callbacks, &descriptor_set_layout) != VK_SUCCESS) {
				printf("Creating descriptor set layout failed.\n");
				exit(-1);
			}
		}

		VkPushConstantRange range = {};
		range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		range.offset = 0;
		range.size = 4;

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

	}

	//Create render pass
	{
		VkAttachmentDescription2 attach_info = {};
		attach_info.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
		attach_info.format = VK_FORMAT_B8G8R8A8_SRGB;
		attach_info.samples = VK_SAMPLE_COUNT_1_BIT;
		attach_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attach_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attach_info.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attach_info.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attach_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attach_info.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;


		VkRenderPassCreateInfo2 info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;


		//vkCreateRenderPass2();
	}
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