#include "VulkanGraphicsDevice.h"

VulkanGraphicsDevice::VulkanGraphicsDevice() {
	Timer timer = Timer("VGD initialization");

	alloc_callbacks = nullptr;			//TODO: Custom allocator(s)
	vma_alloc_callbacks = nullptr;

	_buffers.alloc(256);
	available_images.alloc(1024 * 1024);
	_pending_images.alloc(1024 * 1024);
	_image_upload_batches.alloc(1024);
	_render_passes.alloc(32);
	_graphics_pipelines.alloc(32);
	_immutable_samplers = std::vector<VkSampler>();

	//Initialize volk
	if (volkInitialize() != VK_SUCCESS) {
		printf("RIP\n");
		exit(-1);
	}
	timer.print("Volk initialization");
	timer.start();

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
		inst_info.enabledExtensionCount = static_cast<uint32_t>(extension_names.size());
		inst_info.ppEnabledExtensionNames = extension_names.data();

		timer.print("Vulkan instance params");
		timer.start();

		if (vkCreateInstance(&inst_info, alloc_callbacks, &instance) != VK_SUCCESS) {
			printf("Instance creation failed.\n");
			exit(-1);
		}
		timer.print("just the vkCreateInstance() call");
		timer.start();
	}

	//Load all Vulkan instance functions
	volkLoadInstanceOnly(instance);

	//Vulkan physical device selection
	VkPhysicalDeviceTimelineSemaphoreFeatures semaphore_features = {};
	VkPhysicalDeviceSynchronization2Features sync2_features = {};
	VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features = {};
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

					if (props.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
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
				descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

				sync2_features.pNext = &descriptor_indexing_features;
				semaphore_features.pNext = &sync2_features;
				device_features.pNext = &semaphore_features;
				
				vkGetPhysicalDeviceFeatures2(physical_device, &device_features);

				if (!(descriptor_indexing_features.runtimeDescriptorArray && descriptor_indexing_features.descriptorBindingPartiallyBound)) {
					printf("No support for bindless resource management on this device.\n");
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

	timer.print("Physical device selection");
	timer.start();

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
		device_info.queueCreateInfoCount = static_cast<uint32_t>(queue_infos.size());
		device_info.pQueueCreateInfos = queue_infos.data();
		device_info.enabledLayerCount = 0;
		device_info.ppEnabledLayerNames = nullptr;
		device_info.enabledExtensionCount = static_cast<uint32_t>(extension_names.size());
		device_info.ppEnabledExtensionNames = extension_names.data();
		device_info.pEnabledFeatures = nullptr;

		timer.start();
		if (vkCreateDevice(physical_device, &device_info, alloc_callbacks, &device) != VK_SUCCESS) {
			printf("Creating logical device failed.\n");
			exit(-1);
		}
		timer.print("Just calling vkCreateDevice()");
		timer.start();
	}

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
	timer.print("VMA initialized");
	timer.start();

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
	timer.print("Graphics command pool creation");
	timer.start();

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
	timer.print("Transfer command pool creation");
	timer.start();

	{
		std::vector<VkCommandBuffer> storage;
		storage.resize(128);

		VkCommandBufferAllocateInfo cb_info = {};
		cb_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cb_info.commandPool = graphics_command_pool;
		cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cb_info.commandBufferCount = static_cast<uint32_t>(storage.size());

		if (vkAllocateCommandBuffers(device, &cb_info, storage.data()) != VK_SUCCESS) {
			printf("Creating main command buffers failed.\n");
			exit(-1);
		}

		_graphics_command_buffers = std::stack<VkCommandBuffer, std::vector<VkCommandBuffer>>(storage);
	}

	{
		std::vector<VkCommandBuffer> storage;
		storage.resize(128);

		VkCommandBufferAllocateInfo cb_info = {};
		cb_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cb_info.commandPool = transfer_command_pool;
		cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cb_info.commandBufferCount = static_cast<uint32_t>(storage.size());

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
	timer.print("Command buffer allocation");
	timer.start();

	image_upload_semaphore = create_timeline_semaphore(0);

	//Create pipeline cache
	{
		VkPipelineCacheCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

		std::vector<uint8_t> cache_data;
		if (std::filesystem::exists(PIPELINE_CACHE_FILENAME)) {
			printf("Found pipeline cache file.\n");
			uint32_t pipeline_size = static_cast<uint32_t>(std::filesystem::file_size(PIPELINE_CACHE_FILENAME));
			cache_data.resize(pipeline_size);
			FILE* f = fopen(PIPELINE_CACHE_FILENAME, "rb");
			if (fread(cache_data.data(), 1, pipeline_size, f) == 0) {
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
	timer.print("Pipeline cache creation");
	timer.start();

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
				.maxAnisotropy = 16.0,
				.minLod = 0.0,
				.maxLod = VK_LOD_CLAMP_NONE,
			};

			if (vkCreateSampler(device, &info, alloc_callbacks, &sampler) != VK_SUCCESS) {
				printf("Creating sampler failed.\n");
				exit(-1);
			}
		}
		_immutable_samplers.push_back(sampler);

		//Descriptor set layout
		{
			std::vector<VkDescriptorSetLayoutBinding> bindings;

			VkDescriptorSetLayoutBinding sampled_image_binding = {
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
				.descriptorCount = 1024*1024,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			};
			bindings.push_back(sampled_image_binding);

			VkDescriptorSetLayoutBinding sampler_binding = {
				.binding = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
				.descriptorCount = static_cast<uint32_t>(_immutable_samplers.size()),
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers = _immutable_samplers.data()
			};
			bindings.push_back(sampler_binding);

			std::vector<VkDescriptorBindingFlags> binding_flags;
			binding_flags.reserve(bindings.size());
			binding_flags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
			binding_flags.push_back(0);
			VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info = {
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
				.bindingCount = static_cast<uint32_t>(binding_flags.size()),
				.pBindingFlags = binding_flags.data()
			};

			VkDescriptorSetLayoutCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			info.pNext = &binding_flags_info;
			info.bindingCount = static_cast<uint32_t>(bindings.size());
			info.pBindings = bindings.data();

			if (vkCreateDescriptorSetLayout(device, &info, alloc_callbacks, &descriptor_set_layout) != VK_SUCCESS) {
				printf("Creating descriptor set layout failed.\n");
				exit(-1);
			}
		}

		VkPushConstantRange range;
		range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		range.offset = 0;
		range.size = 16;

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
	timer.print("Pipeline layout creation");
	timer.start();

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
	timer.print("Descriptor pool/set creation");
	timer.start();

	//Start the dedicated image loading thread
	_image_upload_thread = std::thread(
		&VulkanGraphicsDevice::load_images_impl,
		this
	);
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
		if (fwrite(cache_data.data(), 1, data_size, f) == 0) {
			printf("Error writing pipeline cache data.\n");
			exit(-1);
		}
		fclose(f);
	}
	
	for (uint32_t i = 0; i < _image_upload_batches.count(); i++) {
		static uint32_t seen = 0;
		if (!_image_upload_batches.is_live(i)) continue;
		seen += 1;

		VulkanImageUploadBatch* im = _image_upload_batches.data() + i;

		auto buffer = _buffers.get(im->staging_buffer_id);
		vmaDestroyBuffer(allocator, buffer->buffer, buffer->allocation);
	}
	
	for (uint32_t i = 0; i < available_images.count(); i++) {
		static uint32_t seen = 0;
		if (!available_images.is_live(i)) continue;
		seen += 1;

		VulkanAvailableImage* im = available_images.data() + i;

		vkDestroyImageView(device, im->vk_image.image_view, alloc_callbacks);
		vmaDestroyImage(allocator, im->vk_image.image, im->vk_image.image_allocation);
	}
	
	for (uint32_t i = 0; i < _pending_images.count(); i++) {
		static uint32_t seen = 0;
		if (!_pending_images.is_live(i)) continue;
		seen += 1;

		VulkanPendingImage* im = _pending_images.data() + i;

		vkDestroyImageView(device, im->vk_image.image_view, alloc_callbacks);
		vmaDestroyImage(allocator, im->vk_image.image, im->vk_image.image_allocation);
	}
	
	for (uint32_t i = 0; i < _immutable_samplers.size(); i++) {
		vkDestroySampler(device, _immutable_samplers[i], alloc_callbacks);
	}

	uint32_t rp_seen = 0;
	for (uint32_t i = 0; rp_seen < _render_passes.count(); i++) {
		if (!_render_passes.is_live(i)) continue;
		rp_seen += 1;

		vkDestroyRenderPass(device, _render_passes.data()[i], alloc_callbacks);
	}

	//Wait for the image upload thread
	_running = false;
	_image_upload_thread.join();
	
	vkDestroySemaphore(device, image_upload_semaphore, alloc_callbacks);

	vkDestroyPipelineLayout(device, pipeline_layout, alloc_callbacks);
	vkDestroyDescriptorPool(device, descriptor_pool, alloc_callbacks);
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

void VulkanGraphicsDevice::return_transfer_command_buffer(VkCommandBuffer cb) {
	_transfer_command_buffers.push(cb);
}

void VulkanGraphicsDevice::create_graphics_pipelines(
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
) {
	//Non-varying pipeline elements

	//Vertex input state
	//Doing vertex pulling so this can be mostly null :)
	VkPipelineVertexInputStateCreateInfo vert_input_info = {};
	vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	//Dynamic state info
	VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamic_info = {};
	dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_info.dynamicStateCount = 2;
	dynamic_info.pDynamicStates = dyn_states;

	std::vector<VkPipelineShaderStageCreateInfo> shader_stage_infos;
	shader_stage_infos.reserve(2 * pipeline_count);

	std::vector<VkPipelineInputAssemblyStateCreateInfo> ia_infos;
	ia_infos.reserve(pipeline_count);

	std::vector<VkPipelineTessellationStateCreateInfo> tess_infos;
	tess_infos.reserve(pipeline_count);

	std::vector<VkPipelineViewportStateCreateInfo> vs_infos;
	vs_infos.reserve(pipeline_count);

	std::vector<VkPipelineRasterizationStateCreateInfo> rast_infos;
	rast_infos.reserve(pipeline_count);

	std::vector<VkPipelineMultisampleStateCreateInfo> multi_infos;
	multi_infos.reserve(pipeline_count);

	std::vector<VkPipelineDepthStencilStateCreateInfo> ds_infos;
	ds_infos.reserve(pipeline_count);

	std::vector<VkPipelineColorBlendStateCreateInfo> blend_infos;
	blend_infos.reserve(pipeline_count);

	std::vector<VkGraphicsPipelineCreateInfo> pipeline_infos;
	pipeline_infos.reserve(pipeline_count);

	std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states;
	{
		uint32_t total_attachment_count = 0;
		for (uint32_t i = 0; i < pipeline_count; i++) {
			total_attachment_count += blend_state[i].attachmentCount;
		}
		blend_attachment_states.reserve(total_attachment_count);
	}

	//Varying pipeline elements
	uint32_t blend_attachment_state_offset = 0;
	for (uint32_t i = 0; i < pipeline_count; i++) {

		//Shader stages
		//TODO: This assumes that graphics pipelines just have a vertex and fragment shader, but it might be nice to support hardware tessellation. Idk.
		VkShaderModule vertex_shader = this->load_shader_module(shaderfiles[2 * i]);
		VkShaderModule fragment_shader = this->load_shader_module(shaderfiles[2 * i + 1]);
		{
			VkPipelineShaderStageCreateInfo vert_info = {};
			vert_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vert_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vert_info.module = vertex_shader;
			vert_info.pName = "main";
			shader_stage_infos.push_back(vert_info);

			VkPipelineShaderStageCreateInfo frag_info = {};
			frag_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			frag_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			frag_info.module = fragment_shader;
			frag_info.pName = "main";
			shader_stage_infos.push_back(frag_info);
		}
		const VkPipelineShaderStageCreateInfo* shader_stage_ptr = shader_stage_infos.data() + 2 * i;

		//Input assembly state
		VkPipelineInputAssemblyStateCreateInfo ia_info = {};
		ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia_info.topology = ia_state[i].topology;
		ia_info.flags = ia_state[i].flags;
		ia_info.primitiveRestartEnable = ia_state[i].primitiveRestartEnable;
		ia_infos.push_back(ia_info);

		//Tesselation state
		VkPipelineTessellationStateCreateInfo tess_info = {};
		tess_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
		tess_info.flags = tess_state[i].flags;
		tess_info.patchControlPoints = tess_state[i].patchControlPoints;
		tess_infos.push_back(tess_info);

		//Viewport and scissor state
		//These will _always_ be dynamic pipeline states
		VkPipelineViewportStateCreateInfo viewport_info = {};
		viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_info.flags = vp_state[i].flags;
		viewport_info.viewportCount = vp_state[i].viewportCount;
		viewport_info.pViewports = vp_state[i].pViewports;
		viewport_info.scissorCount = vp_state[i].scissorCount;
		viewport_info.pScissors = vp_state[i].pScissors;
		vs_infos.push_back(viewport_info);

		//Rasterization state info
		VkPipelineRasterizationStateCreateInfo rast_info = {};
		rast_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rast_info.flags = raster_state[i].flags;
		rast_info.depthClampEnable = raster_state[i].depthClampEnable;
		rast_info.rasterizerDiscardEnable = raster_state[i].rasterizerDiscardEnable;
		rast_info.polygonMode = raster_state[i].polygonMode;
		rast_info.cullMode = raster_state[i].cullMode;
		rast_info.frontFace = raster_state[i].frontFace;
		rast_info.depthBiasEnable = raster_state[i].depthBiasEnable;
		rast_info.depthBiasClamp = raster_state[i].depthBiasClamp;
		rast_info.depthBiasSlopeFactor = raster_state[i].depthBiasSlopeFactor;
		rast_info.depthBiasConstantFactor = raster_state[i].depthBiasConstantFactor;
		rast_info.lineWidth = raster_state[i].lineWidth;
		rast_infos.push_back(rast_info);

		//Multisample state
		VkPipelineMultisampleStateCreateInfo multi_info = {};
		multi_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multi_info.flags = ms_state[i].flags;
		multi_info.rasterizationSamples = ms_state[i].rasterizationSamples;
		multi_info.sampleShadingEnable = ms_state[i].sampleShadingEnable;
		multi_info.pSampleMask = ms_state[i].pSampleMask;
		multi_info.minSampleShading = ms_state[i].minSampleShading;
		multi_info.alphaToCoverageEnable = ms_state[i].alphaToCoverageEnable,
		multi_info.alphaToOneEnable = ms_state[i].alphaToOneEnable;
		multi_infos.push_back(multi_info);

		//Depth stencil state
		VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {};
		depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_info.flags = ds_state[i].flags;
		depth_stencil_info.depthTestEnable = ds_state[i].depthTestEnable;
		depth_stencil_info.depthWriteEnable = ds_state[i].depthWriteEnable;
		depth_stencil_info.depthCompareOp = ds_state[i].depthCompareOp;
		depth_stencil_info.depthBoundsTestEnable = ds_state[i].depthBoundsTestEnable;
		depth_stencil_info.stencilTestEnable = ds_state[i].stencilTestEnable;
		depth_stencil_info.front = ds_state[i].front;
		depth_stencil_info.back = ds_state[i].back;
		depth_stencil_info.minDepthBounds = ds_state[i].minDepthBounds;
		depth_stencil_info.maxDepthBounds = ds_state[i].maxDepthBounds;
		ds_infos.push_back(depth_stencil_info);

		uint32_t current_attachment_count = blend_state[i].attachmentCount;
		for (uint32_t j = 0; j < current_attachment_count; j++) {
			const VulkanColorBlendAttachmentState state = blend_state[i].pAttachments[j];

			VkPipelineColorBlendAttachmentState blend_attachment_state = {};
			blend_attachment_state.blendEnable = state.blendEnable;
			blend_attachment_state.srcColorBlendFactor = state.srcColorBlendFactor;
			blend_attachment_state.dstColorBlendFactor = state.dstColorBlendFactor;
			blend_attachment_state.colorBlendOp = state.colorBlendOp;
			blend_attachment_state.srcAlphaBlendFactor = state.srcAlphaBlendFactor;
			blend_attachment_state.dstAlphaBlendFactor = state.dstAlphaBlendFactor;
			blend_attachment_state.alphaBlendOp = state.alphaBlendOp;
			blend_attachment_state.colorWriteMask = state.colorWriteMask;
			blend_attachment_states.push_back(blend_attachment_state);
		}
		VkPipelineColorBlendAttachmentState* blend_attachment_state_ptr = blend_attachment_states.data() + blend_attachment_state_offset;
		blend_attachment_state_offset += current_attachment_count;

		VkPipelineColorBlendStateCreateInfo blend_info = {};
		blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blend_info.flags = blend_state[i].flags;
		blend_info.logicOpEnable = blend_state[i].logicOpEnable;
		blend_info.logicOp = blend_state[i].logicOp;
		blend_info.attachmentCount = current_attachment_count;
		blend_info.pAttachments = blend_attachment_state_ptr;
		blend_info.blendConstants[0] = blend_state[i].blendConstants[0];
		blend_info.blendConstants[1] = blend_state[i].blendConstants[1];
		blend_info.blendConstants[2] = blend_state[i].blendConstants[2];
		blend_info.blendConstants[3] = blend_state[i].blendConstants[3];
		blend_infos.push_back(blend_info);

		VkGraphicsPipelineCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		info.stageCount = 2;
		info.pStages = shader_stage_ptr;
		info.pVertexInputState = &vert_input_info;
		info.pInputAssemblyState = &ia_infos[i];
		info.pTessellationState = &tess_infos[i];
		info.pViewportState = &vs_infos[i];
		info.pRasterizationState = &rast_infos[i];
		info.pMultisampleState = &multi_infos[i];
		info.pDepthStencilState = &ds_infos[i];
		info.pColorBlendState = &blend_infos[i];
		info.pDynamicState = &dynamic_info;
		info.layout = pipeline_layout;
		info.renderPass = *_render_passes.get(render_pass_handle);
		info.subpass = 0;
		pipeline_infos.push_back(info);
	}

	std::vector<VkPipeline> pipelines;
	pipelines.resize(pipeline_count);

	if (vkCreateGraphicsPipelines(device, pipeline_cache, pipeline_count, pipeline_infos.data(), alloc_callbacks, pipelines.data()) != VK_SUCCESS) {
		printf("Creating graphics pipeline.\n");
		exit(-1);
	}

	for (uint32_t i = 0; i < pipeline_count; i++) {
		out_pipelines_handles[i] = _graphics_pipelines.insert({
			.pipeline = pipelines[i]
		});

		vkDestroyShaderModule(device, shader_stage_infos[2 * i].module, alloc_callbacks);
		vkDestroyShaderModule(device, shader_stage_infos[2 * i + 1].module, alloc_callbacks);
	}
}

void VulkanGraphicsDevice::record_image_upload_batch(uint64_t id, const std::vector<RawImage>& raw_images, const std::vector<VkFormat>& image_formats) {
	VulkanImageUploadBatch current_batch = {};

	uint32_t image_count = raw_images.size();
	std::vector<uint32_t> mip_counts;
	mip_counts.reserve(image_count);
	VkDeviceSize total_staging_size = 0;
	int channels = 4;
	for (uint32_t i = 0; i < image_count; i++) {
		//Compute number of mips this image will have
		uint32_t mip_count = 1;
		int max_dimension = std::max(raw_images[i].width, raw_images[i].height);
		while (max_dimension >>= 1) {
			mip_count += 1;
		}
		mip_counts.push_back(mip_count);

		total_staging_size += raw_images[i].width * raw_images[i].height * channels;
	}

	//Create staging buffer
	VkBuffer staging_buffer;
	VmaAllocation staging_buffer_allocation;
	VmaAllocationInfo sb_alloc_info;
	{
		VkBufferCreateInfo buffer_info = {};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = total_staging_size;
		buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		buffer_info.queueFamilyIndexCount = 1;
		buffer_info.pQueueFamilyIndices = &transfer_queue_family_idx;

		VmaAllocationCreateInfo alloc_info = {};
		alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
		alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		alloc_info.priority = 1.0;

		if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info, &staging_buffer, &staging_buffer_allocation, &sb_alloc_info) != VK_SUCCESS) {
			printf("Creating staging buffer failed.\n");
			exit(-1);
		}

		current_batch.staging_buffer_id = _buffers.insert({
			.buffer = staging_buffer,
			.allocation = staging_buffer_allocation
		});
	}

	//Copy image data to staging buffer
	{
		uint8_t* mapped_head = static_cast<uint8_t*>(sb_alloc_info.pMappedData);

		for (uint32_t i = 0; i < image_count; i++) {
			void* image_bytes = raw_images[i].data;
			int x = raw_images[i].width;
			int y = raw_images[i].height;
			size_t num_bytes = static_cast<size_t>(x * y * channels);

			memcpy(mapped_head, image_bytes, num_bytes);
			free(image_bytes);

			mapped_head += num_bytes;
		}
	}
	
	//Create Vulkan images
	std::vector<VkImage> images;
	std::vector<VkImageView> image_views;
	std::vector<VmaAllocation> image_allocations;
	images.resize(image_count);
	image_views.resize(image_count);
	image_allocations.resize(image_count);
	for (uint32_t i = 0; i < image_count; i++) {
		//Create image
		{
			VkImageCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			info.imageType = VK_IMAGE_TYPE_2D;
			info.format = image_formats[i];
			info.extent = {
				.width = static_cast<uint32_t>(raw_images[i].width),
				.height = static_cast<uint32_t>(raw_images[i].height),
				.depth = 1
			};
			info.mipLevels = mip_counts[i];
			info.arrayLayers = 1;
			info.samples = VK_SAMPLE_COUNT_1_BIT;
			info.tiling = VK_IMAGE_TILING_OPTIMAL;
			info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			info.queueFamilyIndexCount = 1;
			info.pQueueFamilyIndices = &transfer_queue_family_idx;
			info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			VmaAllocationCreateInfo alloc_info = {};
			alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
			alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
			alloc_info.priority = 1.0;

			if (vmaCreateImage(allocator, &info, &alloc_info, &images[i], &image_allocations[i], nullptr) != VK_SUCCESS) {
				printf("Creating staging buffer failed.\n");
				exit(-1);
			}
		}

		//Create image view
		{
			VkImageSubresourceRange subresource_range = {};
			subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresource_range.baseMipLevel = 0;
			subresource_range.levelCount = mip_counts[i];
			subresource_range.baseArrayLayer = 0;
			subresource_range.layerCount = 1;

			VkImageViewCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			info.image = images[i];
			info.viewType = VK_IMAGE_VIEW_TYPE_2D;
			info.format = image_formats[i];
			info.components = COMPONENT_MAPPING_DEFAULT;
			info.subresourceRange = subresource_range;

			if (vkCreateImageView(device, &info, alloc_callbacks, &image_views[i]) != VK_SUCCESS) {
				printf("Creating image view failed.\n");
				exit(-1);
			}
		}
	}

	//Record CopyBufferToImage commands along with relevant barriers
	{
		current_batch.command_buffer = borrow_transfer_command_buffer();

		//Begin command buffer
		{
			VkCommandBufferBeginInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

			vkBeginCommandBuffer(current_batch.command_buffer, &info);
		}

		//Record barrier to transition into optimal transfer dst layout
		for (uint32_t i = 0; i < image_count; i++) {
			VkImageMemoryBarrier2KHR barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
			barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT_KHR;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

			barrier.image = images[i];
			barrier.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = mip_counts[i],
				.baseArrayLayer = 0,
				.layerCount = 1
			};

			VkDependencyInfoKHR info = {};
			info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			info.imageMemoryBarrierCount = 1;
			info.pImageMemoryBarriers = &barrier;

			vkCmdPipelineBarrier2KHR(current_batch.command_buffer, &info);
		}

		//Record buffer copy to image
		VkDeviceSize copy_offset = 0;
		for (uint32_t i = 0; i < image_count; i++) {
			uint32_t x = static_cast<uint32_t>(raw_images[i].width);
			uint32_t y = static_cast<uint32_t>(raw_images[i].height);
			VkBufferImageCopy region = {
				.bufferOffset = copy_offset,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1
				},
				.imageOffset = 0,
				.imageExtent = {
					.width = x,
					.height = y,
					.depth = 1
				}
			};

			copy_offset += x * y * channels;

			vkCmdCopyBufferToImage(current_batch.command_buffer, staging_buffer, images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		//Queue ownership transfer
		for (uint32_t i = 0; i < image_count; i++) {
			VkImageMemoryBarrier2KHR barriers[] = {
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
					.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
					.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
					.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
					.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,

					.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					.srcQueueFamilyIndex = transfer_queue_family_idx,
					.dstQueueFamilyIndex = graphics_queue_family_idx,
					.image = images[i],
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				},
				{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
					.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
					.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
					.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
					.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,

					.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
					.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					.srcQueueFamilyIndex = transfer_queue_family_idx,
					.dstQueueFamilyIndex = graphics_queue_family_idx,
					.image = images[i],
					.subresourceRange = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 1,
						.levelCount = mip_counts[i] - 1,
						.baseArrayLayer = 0,
						.layerCount = 1
					}
				}
			};

			VkDependencyInfoKHR info = {};
			info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			info.imageMemoryBarrierCount = 2;
			info.pImageMemoryBarriers = barriers;

			vkCmdPipelineBarrier2KHR(current_batch.command_buffer, &info);
		}

		vkEndCommandBuffer(current_batch.command_buffer);
	}

	//Submit upload command buffer
	VkQueue q;
	vkGetDeviceQueue(device, transfer_queue_family_idx, 0, &q);
	{
		uint64_t signal_value = id;
		VkTimelineSemaphoreSubmitInfo ts_info = {};
		ts_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		ts_info.signalSemaphoreValueCount = 1;
		ts_info.pSignalSemaphoreValues = &signal_value;

		VkPipelineStageFlags flags[] = { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0 };
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.pNext = &ts_info;
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &image_upload_semaphore;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &current_batch.command_buffer;
		info.pWaitDstStageMask = flags;

		if (vkQueueSubmit(q, 1, &info, VK_NULL_HANDLE) != VK_SUCCESS) {
			printf("Queue submit failed.\n");
			exit(-1);
		}
		current_batch.id = signal_value;

		_image_upload_mutex.lock();
		_image_upload_batches.insert(current_batch);
		_image_upload_mutex.unlock();
	}

	//Insert into pending images table
	for (uint32_t i = 0; i < image_count; i++) {
		VulkanPendingImage pending_image = {};
		pending_image.vk_image.image = images[i];
		pending_image.vk_image.image_view = image_views[i];
		pending_image.vk_image.image_allocation = image_allocations[i];
		pending_image.batch_id = current_batch.id;
		pending_image.vk_image.mip_levels = mip_counts[i];
		pending_image.vk_image.x = raw_images[i].width;
		pending_image.vk_image.y = raw_images[i].height;
		pending_image.vk_image.z = 1;

		_pending_image_mutex.lock();
		_pending_images.insert(pending_image);
		_pending_image_mutex.unlock();
	}
}

//TODO: Just what is even happening with the image format :( UNORM works for everything??!???

uint64_t VulkanGraphicsDevice::load_raw_images(
	const std::vector<RawImage> raw_images,
	const std::vector<VkFormat> image_formats
) {
	_image_uploads_requested += 1;
	_raw_image_mutex.lock();
	_raw_image_batch_queue.push({
		.id = _image_uploads_requested,
		.raw_images = raw_images,
		.image_formats = image_formats
	});
	_raw_image_mutex.unlock();

	return _image_uploads_requested;
}

uint64_t VulkanGraphicsDevice::load_image_files(
	const std::vector<const char*> filenames,
	const std::vector<VkFormat> image_formats
) {
	_image_uploads_requested += 1;
	_file_batch_mutex.lock();
	_image_file_batch_queue.push({
		.id = _image_uploads_requested,
		.filenames = std::move(filenames),
		.image_formats = std::move(image_formats)
	});
	_file_batch_mutex.unlock();

	return _image_uploads_requested;
}

//Main function for image loading thread
void VulkanGraphicsDevice::load_images_impl() {
	while (_running) {
		//Loading images from memory
		while (_raw_image_batch_queue.size() > 0) {
			RawImageBatchParameters& params = _raw_image_batch_queue.front();
			uint32_t image_count = params.raw_images.size();

			this->record_image_upload_batch(params.id, params.raw_images, params.image_formats);

			//Remove processed parameters
			_raw_image_mutex.lock();
			_raw_image_batch_queue.pop();
			_raw_image_mutex.unlock();
		}

		//Loading images from files
		while (_image_file_batch_queue.size() > 0) {
			FileImageBatchParameters params = _image_file_batch_queue.front();
			uint32_t image_count = params.filenames.size();

			//Load image data from disk
			std::vector<RawImage> raw_images;
			raw_images.resize(image_count);
			for (uint32_t i = 0; i < image_count; i++) {
				int width, height;
				raw_images[i].data = stbi_load(params.filenames[i], &width, &height, nullptr, STBI_rgb_alpha);
				raw_images[i].width = static_cast<uint32_t>(width);
				raw_images[i].height = static_cast<uint32_t>(height);
				
				if (!raw_images[i].data) {
					printf("Loading image failed.\n");
					exit(-1);
				}
			}

			this->record_image_upload_batch(params.id, raw_images, params.image_formats);

			//Remove processed parameters
			_file_batch_mutex.lock();
			_image_file_batch_queue.pop();
			_file_batch_mutex.unlock();
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}

void VulkanGraphicsDevice::tick_image_uploads(VkCommandBuffer render_cb) {
	if (!_pending_image_mutex.try_lock())
		return;

	if (!_image_upload_mutex.try_lock()) {
		_pending_image_mutex.unlock();
		return;
	}

	uint64_t gpu_batches_processed = check_timeline_semaphore(image_upload_semaphore);

	//Descriptor update state
	std::vector<VkDescriptorImageInfo> desc_infos;
	std::vector<VkWriteDescriptorSet> desc_writes;
	desc_infos.reserve(16);
	desc_writes.reserve(16);

	uint32_t seen = 0;
	const uint32_t count = _image_upload_batches.count();
	for (uint32_t i = 0; seen < count; i++) {
		if (!_image_upload_batches.is_live(i)) continue;
		seen += 1;
		VulkanImageUploadBatch* batch = _image_upload_batches.data() + i;
		if (batch->id > gpu_batches_processed) continue;

		//Make the transfer command buffer available again and destroy the staging buffer
		return_transfer_command_buffer(batch->command_buffer);
		vmaDestroyBuffer(allocator, _buffers.get(batch->staging_buffer_id)->buffer, _buffers.get(batch->staging_buffer_id)->allocation);

		uint32_t seen_images = 0;
		uint32_t start_count = _pending_images.count();
		for (uint32_t j = 0; seen_images < start_count; j++) {
			if (!_pending_images.is_live(j)) continue;
			seen_images += 1;

			VulkanPendingImage* pending_image = _pending_images.data() + j;

			if (pending_image->batch_id == batch->id) {
				//Record Graphics queue acquire ownership of the image
				{
					VkImageMemoryBarrier2KHR barriers[] = {
						{
							.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
							.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
							.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
							.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
							.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,

							.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							.srcQueueFamilyIndex = transfer_queue_family_idx,
							.dstQueueFamilyIndex = graphics_queue_family_idx,
							.image = pending_image->vk_image.image,
							.subresourceRange = {
								.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
								.baseMipLevel = 0,
								.levelCount = 1,
								.baseArrayLayer = 0,
								.layerCount = 1
							}
						},
						{
							.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
							.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
							.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
							.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
							.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,

							.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
							.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							.srcQueueFamilyIndex = transfer_queue_family_idx,
							.dstQueueFamilyIndex = graphics_queue_family_idx,
							.image = pending_image->vk_image.image,
							.subresourceRange = {
								.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
								.baseMipLevel = 1,
								.levelCount = pending_image->vk_image.mip_levels - 1,
								.baseArrayLayer = 0,
								.layerCount = 1
							}
						}
					};

					VkDependencyInfoKHR info = {};
					info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
					info.imageMemoryBarrierCount = 2;
					info.pImageMemoryBarriers = barriers;

					vkCmdPipelineBarrier2KHR(render_cb, &info);
				}

				//Record mipmapping commands
				if (pending_image->vk_image.mip_levels > 1) {
					for (uint32_t k = 0; k < pending_image->vk_image.mip_levels - 1; k++) {
						VkImageBlit region = {
							.srcSubresource = {
								.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
								.mipLevel = k,
								.baseArrayLayer = 0,
								.layerCount = 1,
							},
							.srcOffsets = {
								{
									.x = 0,
									.y = 0,
									.z = 0,
								},
								{
									.x = static_cast<int32_t>(pending_image->vk_image.x >> k),
									.y = static_cast<int32_t>(pending_image->vk_image.y >> k),
									.z = 1,
								}
							},
							.dstSubresource = {
								.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
								.mipLevel = k + 1,
								.baseArrayLayer = 0,
								.layerCount = 1
							},
							.dstOffsets = {
								{
									.x = 0,
									.y = 0,
									.z = 0,
								},
								{
									.x = static_cast<int32_t>(pending_image->vk_image.x >> (k + 1)),
									.y = static_cast<int32_t>(pending_image->vk_image.y >> (k + 1)),
									.z = 1,
								}
							},
						};

						vkCmdBlitImage(
							render_cb,
							pending_image->vk_image.image,
							VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							pending_image->vk_image.image,
							VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							1,
							&region,
							VK_FILTER_LINEAR
						);

						{
							VkImageMemoryBarrier2KHR barriers[] = {
								{
									.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
									.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
									.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
									.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
									.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR,
									.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
									.image = pending_image->vk_image.image,
									.subresourceRange = {
										.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
										.baseMipLevel = k,
										.levelCount = 1,
										.baseArrayLayer = 0,
										.layerCount = 1,
									}
								},
								{
									.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
									.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
									.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
									.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
									.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR,
									.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									.image = pending_image->vk_image.image,
									.subresourceRange = {
										.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
										.baseMipLevel = k + 1,
										.levelCount = 1,
										.baseArrayLayer = 0,
										.layerCount = 1,
									}
								}
							};
							VkDependencyInfoKHR info = {
								.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
								.imageMemoryBarrierCount = 2,
								.pImageMemoryBarriers = barriers
							};
							vkCmdPipelineBarrier2KHR(render_cb, &info);
						}
					}

					//Final barrier
					{
						VkImageMemoryBarrier2KHR barriers[] = {
							{
								.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
								.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR,
								.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
								.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT_KHR,
								.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR,
								.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
								.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
								.image = pending_image->vk_image.image,
								.subresourceRange = {
									.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
									.baseMipLevel = pending_image->vk_image.mip_levels - 1,
									.levelCount = 1,
									.baseArrayLayer = 0,
									.layerCount = 1,
								}
							}
						};
						VkDependencyInfoKHR info = {
							.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR,
							.imageMemoryBarrierCount = 1,
							.pImageMemoryBarriers = barriers
						};
						vkCmdPipelineBarrier2KHR(render_cb, &info);
					}
				}

				//Descriptor update data
				{
					VulkanAvailableImage ava = {};
					ava.batch_id = batch->id;
					ava.vk_image = pending_image->vk_image;
					
					uint64_t handle = available_images.insert(ava);
					_pending_images.remove(j);

					uint32_t descriptor_index = EXTRACT_IDX(handle);

					VkDescriptorImageInfo info = {
						.imageView = ava.vk_image.image_view,
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};
					desc_infos.push_back(info);
					VkWriteDescriptorSet write = {
						.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						.dstSet = descriptor_set,
						.dstBinding = 0,
						.dstArrayElement = descriptor_index,
						.descriptorCount = 1,
						.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
						.pImageInfo = &desc_infos[desc_infos.size() - 1]
					};
					desc_writes.push_back(write);
				}
			}
		}
		
		_image_uploads_completed += 1;
		_image_upload_batches.remove(i);
	}
	_pending_image_mutex.unlock();
	_image_upload_mutex.unlock();
	
	if (seen > 0)
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(desc_writes.size()), desc_writes.data(), 0, nullptr);
}

uint64_t VulkanGraphicsDevice::get_completed_image_uploads() {
	return _image_uploads_completed;
}

VkSemaphore VulkanGraphicsDevice::create_timeline_semaphore(uint64_t initial_value) {
	VkSemaphoreTypeCreateInfo type_info = {};
	type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	type_info.initialValue = initial_value;

	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = &type_info;

	VkSemaphore ts;
	if (vkCreateSemaphore(device, &info, alloc_callbacks, &ts) != VK_SUCCESS) {
		printf("Creating timeline semaphore failed.\n");
		exit(-1);
	}
	return ts;
}

uint64_t VulkanGraphicsDevice::check_timeline_semaphore(VkSemaphore semaphore) {
	uint64_t value;
	vkGetSemaphoreCounterValue(device, semaphore, &value);
	return value;
}

VulkanGraphicsPipeline* VulkanGraphicsDevice::get_graphics_pipeline(uint64_t handle) {
	return _graphics_pipelines.get(handle);
}

uint64_t VulkanGraphicsDevice::create_render_pass(VkRenderPassCreateInfo& info) {
	VkRenderPass render_pass;	
	if (vkCreateRenderPass(device, &info, alloc_callbacks, &render_pass) != VK_SUCCESS) {
		printf("Creating render pass failed.\n");
		exit(-1);
	}
	return _render_passes.insert(render_pass);
}

VkRenderPass* VulkanGraphicsDevice::get_render_pass(uint64_t key) {
	return _render_passes.get(key);
}

VkShaderModule VulkanGraphicsDevice::load_shader_module(const char* path) {
	//Get filesize
	uint32_t spv_size = static_cast<uint32_t>(std::filesystem::file_size(std::filesystem::path(path)));

	//Allocate receiving buffer
	std::vector<uint32_t> spv_bytes;
	spv_bytes.resize(spv_size);

	//Read spv bytes
	FILE* shader_spv = fopen(path, "rb");
	if (fread(spv_bytes.data(), 1, spv_size, shader_spv) == 0) {
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
		printf("Creating shader module failed.\n");
		exit(-1);
	}

	return shader;
}