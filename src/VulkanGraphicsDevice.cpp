#include "VulkanGraphicsDevice.h"
#include <algorithm>
#include <filesystem>
#include "stb_image.h"
#include "timer.h"
#include "utils.h"

VulkanGraphicsDevice::VulkanGraphicsDevice() {
	Timer timer = Timer("VGD initialization");

	alloc_callbacks = nullptr;			//TODO: Custom allocator(s)
	vma_alloc_callbacks = nullptr;

	_buffers.alloc(1024 * 1024);
	bindless_images.alloc(1024 * 1024);
	_pending_images.alloc(1024 * 1024);
	_image_upload_batches.alloc(1024);
	_framebuffers.alloc(1024);
	_semaphores.alloc(1024);
	_render_passes.alloc(32);
	_graphics_pipelines.alloc(32);

	//Initialize volk
	VKASSERT_OR_CRASH(volkInitialize());
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

		VKASSERT_OR_CRASH(vkCreateInstance(&inst_info, alloc_callbacks, &instance));
		timer.print("just the vkCreateInstance() call");
		timer.start();
	}

	//Load all Vulkan instance functions
	volkLoadInstanceOnly(instance);
	timer.print("volkLoadInstanceOnly()");
	timer.start();

	//Vulkan physical device selection
	VkPhysicalDeviceTimelineSemaphoreFeatures semaphore_features = {};
	VkPhysicalDeviceSynchronization2Features sync2_features = {};
	VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features = {};
	VkPhysicalDeviceBufferDeviceAddressFeatures bda_features = {};
	{
		uint32_t physical_device_count = 0;
		//Getting physical device count by passing nullptr as last param
		VKASSERT_OR_CRASH(vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr));
		printf("%i physical devices available.\n", physical_device_count);

		std::vector<VkPhysicalDevice> devices;
		devices.resize(physical_device_count);
		VKASSERT_OR_CRASH(vkEnumeratePhysicalDevices(instance, &physical_device_count, devices.data()));

		const VkPhysicalDeviceType TYPES[] = { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, VK_PHYSICAL_DEVICE_TYPE_CPU };
		for (uint32_t j = 0; j < 3; j++) {
			for (uint32_t i = 0; i < physical_device_count; i++) {
				VkPhysicalDevice phys_device = devices[i];

				VkPhysicalDeviceProperties2 device_properties = {};
				device_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				vkGetPhysicalDeviceProperties2(phys_device, &device_properties);

				physical_limits = device_properties.properties.limits;

				uint32_t queue_count = 0;
				vkGetPhysicalDeviceQueueFamilyProperties2(phys_device, &queue_count, nullptr);

				std::vector<VkQueueFamilyProperties2> queue_properties;
				queue_properties.resize(queue_count);
				//C++ initialization moment
				for (uint32_t k = 0; k < queue_count; k++) {
					queue_properties[k].pNext = nullptr;
					queue_properties[k].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
				}

				vkGetPhysicalDeviceQueueFamilyProperties2(phys_device, &queue_count, queue_properties.data());

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
					physical_device = phys_device;
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
				bda_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

				//pNext chain setup
				device_features.pNext = &semaphore_features;
				semaphore_features.pNext = &sync2_features;
				sync2_features.pNext = &descriptor_indexing_features;
				descriptor_indexing_features.pNext = &bda_features;
				
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

				if (!bda_features.bufferDeviceAddress) {
					printf("No buffer device address support on this device.\n");
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
		VKASSERT_OR_CRASH(vkCreateDevice(physical_device, &device_info, alloc_callbacks, &device));
		timer.print("Just calling vkCreateDevice()");
		timer.start();
	}

	//Load all device functions
	volkLoadDevice(device);

	//Initialize VMA
	{
		VmaAllocatorCreateInfo info = {};
		info.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
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

		VKASSERT_OR_CRASH(vmaCreateAllocator(&info, &allocator));
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

		VKASSERT_OR_CRASH(vkCreateCommandPool(device, &pool_info, alloc_callbacks, &graphics_command_pool));
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

		VKASSERT_OR_CRASH(vkCreateCommandPool(device, &pool_info, alloc_callbacks, &transfer_command_pool));
	}
	timer.print("Transfer command pool creation");
	timer.start();

	//Allocate graphics command buffers
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

	//Allocate transfer command buffers
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

	//Create desciptor set for sampled images and immutable samplers
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
                _immutable_samplers.push_back(create_sampler(info));

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
                _immutable_samplers.push_back(create_sampler(info));
            }

            std::vector<VulkanDescriptorLayoutBinding> descriptor_sets;

            //Images
            descriptor_sets.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptor_count = 1024*1024,
                .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT
            });

            //Samplers
            descriptor_sets.push_back({
                .descriptor_type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptor_count = static_cast<uint32_t>(_immutable_samplers.size()),
                .stage_flags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .immutable_samplers = _immutable_samplers.data()
            });

			{
				std::vector<VkDescriptorSetLayoutBinding> bindings;
				bindings.reserve(descriptor_sets.size());
				std::vector<VkDescriptorBindingFlags> bindings_flags;
				bindings_flags.reserve(descriptor_sets.size());

				VkDescriptorBindingFlags binding_flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

				for (uint32_t i = 0; i < descriptor_sets.size(); i++) {
					VulkanDescriptorLayoutBinding& b = descriptor_sets[i];
					VkDescriptorSetLayoutBinding binding = {
						.binding = i,
						.descriptorType = b.descriptor_type,
						.descriptorCount = b.descriptor_count,
						.stageFlags = b.stage_flags,
						.pImmutableSamplers = b.immutable_samplers
					};
					bindings.push_back(binding);
					bindings_flags.push_back(binding_flags);
				}

				VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info = {
					.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
					.bindingCount = static_cast<uint32_t>(bindings_flags.size()),
					.pBindingFlags = bindings_flags.data()
				};

				VkDescriptorSetLayoutCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
				info.pNext = &binding_flags_info;
				info.bindingCount = static_cast<uint32_t>(bindings.size());
				info.pBindings = bindings.data();

				if (vkCreateDescriptorSetLayout(device, &info, alloc_callbacks, &_image_descriptor_set_layout) != VK_SUCCESS) {
					printf("Creating descriptor set layout failed.\n");
					exit(-1);
				}
			}
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
                    }
                };

                VkDescriptorPoolCreateInfo info = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                    .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                    .maxSets = 1,
                    .poolSizeCount = 2,
                    .pPoolSizes = sizes
                };

                if (vkCreateDescriptorPool(device, &info, alloc_callbacks, &_descriptor_pool) != VK_SUCCESS) {
                    printf("Creating descriptor pool failed.\n");
                    exit(-1);
                }
            }

            {
                VkDescriptorSetAllocateInfo info = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = _descriptor_pool,
                    .descriptorSetCount = 1,
                    .pSetLayouts = &_image_descriptor_set_layout
                };

                if (vkAllocateDescriptorSets(device, &info, &_image_descriptor_set) != VK_SUCCESS) {
                    printf("Allocating descriptor set failed.\n");
                    exit(-1);
                }
            }
        }

		//Create pipeline layout
		{
			std::vector<VkPushConstantRange> ranges = {
				{
					.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					.offset = 0,
					.size = 128
				}
			};

			VkPipelineLayoutCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			info.setLayoutCount = 1;
			info.pSetLayouts = &_image_descriptor_set_layout;
			info.pushConstantRangeCount = (uint32_t)ranges.size();
			info.pPushConstantRanges = ranges.data();

			if (vkCreatePipelineLayout(device, &info, alloc_callbacks, &_pipeline_layout) != VK_SUCCESS) {
				printf("Creating pipeline layout failed.\n");
				exit(-1);
			}
		}
	}

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

	//Wait for the image upload thread
	_image_upload_running = false;
	_image_upload_thread.join();

	//TODO: It doesn't make sense why I had to do this
	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT + 1; i++)
		service_deletion_queues();

	for (VulkanBindlessImage& im : bindless_images) {
		vkDestroyImageView(device, im.vk_image.image_view, alloc_callbacks);
		vmaDestroyImage(allocator, im.vk_image.image, im.vk_image.image_allocation);
	}

	for (VulkanPendingImage& im : _pending_images) {
		vkDestroyImageView(device, im.vk_image.image_view, alloc_callbacks);
		vmaDestroyImage(allocator, im.vk_image.image, im.vk_image.image_allocation);
	}

	for (VkRenderPass& pass : _render_passes) {
		vkDestroyRenderPass(device, pass, alloc_callbacks);
	}

	for (VulkanGraphicsPipeline& p : _graphics_pipelines) {
		vkDestroyPipeline(device, p.pipeline, alloc_callbacks);
	}

	for (VkSemaphore& s : _semaphores) {
		vkDestroySemaphore(device, s, alloc_callbacks);
	}

	for (VkFramebuffer& fb : _framebuffers) {
		vkDestroyFramebuffer(device, fb, alloc_callbacks);
	}

	for (VkSampler s : _immutable_samplers) {
		vkDestroySampler(device, s, alloc_callbacks);
	}

	vkDestroyDescriptorPool(device, _descriptor_pool, alloc_callbacks);
	vkDestroyPipelineLayout(device, _pipeline_layout, alloc_callbacks);
	vkDestroyDescriptorSetLayout(device, _image_descriptor_set_layout, alloc_callbacks);

	vkDestroyCommandPool(device, transfer_command_pool, alloc_callbacks);
	vkDestroyCommandPool(device, graphics_command_pool, alloc_callbacks);
	vkDestroyPipelineCache(device, pipeline_cache, alloc_callbacks);

	vmaDestroyAllocator(allocator);

	vkDestroyDevice(device, alloc_callbacks);
	vkDestroyInstance(instance, alloc_callbacks);
}

VkCommandBuffer VulkanGraphicsDevice::get_graphics_command_buffer() {
	VkCommandBuffer cb = _graphics_command_buffers.top();
	_graphics_command_buffers.pop();

	VkCommandBufferBeginInfo begin_info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	};
	vkBeginCommandBuffer(cb, &begin_info);

	//Bind bindless descriptor set before returning the command buffer
	vkCmdBindDescriptorSets(
		cb,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		_pipeline_layout,
		0,
		1,
		&_image_descriptor_set,
		0,
		nullptr
	);

	return cb;
}

void VulkanGraphicsDevice::return_command_buffer(VkCommandBuffer cb, uint64_t wait_value, Key<VkSemaphore> wait_semaphore) {
	_command_buffer_returns.push_back({
		.cb = cb,
		.wait_value = wait_value,
		.wait_semaphore = wait_semaphore
	});
}

void VulkanGraphicsDevice::graphics_queue_submit(VkCommandBuffer cb, SyncData& sync_data) {
	//End the command buffer
	vkEndCommandBuffer(cb);

	VkQueue q;
	vkGetDeviceQueue(device, graphics_queue_family_idx, 0, &q);
	{
		sync_data.wait_semaphores.push_back(*_semaphores.get(image_upload_semaphore));
		sync_data.wait_values.push_back(_image_batches_completed);

		uint32_t wait_count = static_cast<uint32_t>(sync_data.wait_semaphores.size());
		uint32_t signal_count = static_cast<uint32_t>(sync_data.signal_semaphores.size());
		VkTimelineSemaphoreSubmitInfo ts_info = {};
		ts_info.waitSemaphoreValueCount = wait_count;
		ts_info.pWaitSemaphoreValues = sync_data.wait_values.data();
		ts_info.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
		ts_info.signalSemaphoreValueCount = signal_count;
		ts_info.pSignalSemaphoreValues = sync_data.signal_values.data();

		// printf("\nNew call to graphics_queue_submit.\n");
		// for (uint32_t i = 0; i < wait_count; ++i) {
		// 	if (sync_data.wait_values[i] == 0) {
		// 		printf("Waiting on binary semaphore.\n");
		// 	} else {
		// 		printf("Waiting on timeline semaphore with value %i.\n", (int)sync_data.wait_values[i]);
		// 	}
		// }

		// for (uint32_t i = 0; i < signal_count; ++i) {
		// 	if (sync_data.signal_values[i] == 0) {
		// 		printf("Signalling binary semaphore.\n");
		// 	} else {
		// 		printf("Signalling timeline semaphore with value %i.\n", (int)sync_data.signal_values[i]);
		// 	}
		// }

		//TODO: Reevaluate this line
		VkPipelineStageFlags wait_flags[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT };

		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.pNext = &ts_info;
		info.waitSemaphoreCount = wait_count;
		info.pWaitSemaphores = sync_data.wait_semaphores.data();
		info.pWaitDstStageMask = wait_flags;
		info.signalSemaphoreCount = signal_count;
		info.pSignalSemaphores = sync_data.signal_semaphores.data();
		info.commandBufferCount = 1;
		info.pCommandBuffers = &cb;

		VKASSERT_OR_CRASH(vkQueueSubmit(q, 1, &info, VK_NULL_HANDLE));
	}
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
	const std::vector<VulkanGraphicsPipelineConfig>& pipeline_configs,
	Key<VulkanGraphicsPipeline>* out_pipelines_handles
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
	shader_stage_infos.reserve(2 * pipeline_configs.size());

	std::vector<VkPipelineInputAssemblyStateCreateInfo> ia_infos;
	ia_infos.reserve(pipeline_configs.size());

	std::vector<VkPipelineTessellationStateCreateInfo> tess_infos;
	tess_infos.reserve(pipeline_configs.size());

	std::vector<VkPipelineViewportStateCreateInfo> vs_infos;
	vs_infos.reserve(pipeline_configs.size());

	std::vector<VkPipelineRasterizationStateCreateInfo> rast_infos;
	rast_infos.reserve(pipeline_configs.size());

	std::vector<VkPipelineMultisampleStateCreateInfo> multi_infos;
	multi_infos.reserve(pipeline_configs.size());

	std::vector<VkPipelineDepthStencilStateCreateInfo> ds_infos;
	ds_infos.reserve(pipeline_configs.size());

	std::vector<VkPipelineColorBlendStateCreateInfo> blend_infos;
	blend_infos.reserve(pipeline_configs.size());

	std::vector<VkGraphicsPipelineCreateInfo> pipeline_infos;
	pipeline_infos.reserve(pipeline_configs.size());

	std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states;
	{
		uint32_t total_attachment_count = 0;
		for (uint32_t i = 0; i < pipeline_configs.size(); i++) {
			total_attachment_count += pipeline_configs[i].blend_state.attachmentCount;
		}
		blend_attachment_states.reserve(total_attachment_count);
	}

	//Varying pipeline elements
	uint32_t blend_attachment_state_offset = 0;
	for (uint32_t i = 0; i < pipeline_configs.size(); i++) {

		//Shader stages
		//TODO: This assumes that graphics pipelines just have a vertex and fragment shader
		VkShaderModule vertex_shader = this->load_shader_module(pipeline_configs[i].spv_sources[0]);
		VkShaderModule fragment_shader = this->load_shader_module(pipeline_configs[i].spv_sources[1]);
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
		ia_info.topology = pipeline_configs[i].input_assembly_state.topology;
		ia_info.flags = pipeline_configs[i].input_assembly_state.flags;
		ia_info.primitiveRestartEnable = pipeline_configs[i].input_assembly_state.primitiveRestartEnable;
		ia_infos.push_back(ia_info);

		//Tesselation state
		VkPipelineTessellationStateCreateInfo tess_info = {};
		tess_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
		tess_info.flags = pipeline_configs[i].tesselation_state.flags;
		tess_info.patchControlPoints = pipeline_configs[i].tesselation_state.patchControlPoints;
		tess_infos.push_back(tess_info);

		//Viewport and scissor state
		//These will _always_ be dynamic pipeline states
		VkPipelineViewportStateCreateInfo viewport_info = {};
		viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_info.flags = pipeline_configs[i].viewport_state.flags;
		viewport_info.viewportCount = pipeline_configs[i].viewport_state.viewportCount;
		viewport_info.pViewports = pipeline_configs[i].viewport_state.pViewports;
		viewport_info.scissorCount = pipeline_configs[i].viewport_state.scissorCount;
		viewport_info.pScissors = pipeline_configs[i].viewport_state.pScissors;
		vs_infos.push_back(viewport_info);

		//Rasterization state info
		VkPipelineRasterizationStateCreateInfo rast_info = {};
		rast_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rast_info.flags = pipeline_configs[i].rasterization_state.flags;
		rast_info.depthClampEnable = pipeline_configs[i].rasterization_state.depthClampEnable;
		rast_info.rasterizerDiscardEnable = pipeline_configs[i].rasterization_state.rasterizerDiscardEnable;
		rast_info.polygonMode = pipeline_configs[i].rasterization_state.polygonMode;
		rast_info.cullMode = pipeline_configs[i].rasterization_state.cullMode;
		rast_info.frontFace = pipeline_configs[i].rasterization_state.frontFace;
		rast_info.depthBiasEnable = pipeline_configs[i].rasterization_state.depthBiasEnable;
		rast_info.depthBiasClamp = pipeline_configs[i].rasterization_state.depthBiasClamp;
		rast_info.depthBiasSlopeFactor = pipeline_configs[i].rasterization_state.depthBiasSlopeFactor;
		rast_info.depthBiasConstantFactor = pipeline_configs[i].rasterization_state.depthBiasConstantFactor;
		rast_info.lineWidth = pipeline_configs[i].rasterization_state.lineWidth;
		rast_infos.push_back(rast_info);

		//Multisample state
		VkPipelineMultisampleStateCreateInfo multi_info = {};
		multi_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multi_info.flags = pipeline_configs[i].multisample_state.flags;
		multi_info.rasterizationSamples = pipeline_configs[i].multisample_state.rasterizationSamples;
		multi_info.sampleShadingEnable = pipeline_configs[i].multisample_state.sampleShadingEnable;
		multi_info.pSampleMask = pipeline_configs[i].multisample_state.pSampleMask;
		multi_info.minSampleShading = pipeline_configs[i].multisample_state.minSampleShading;
		multi_info.alphaToCoverageEnable = pipeline_configs[i].multisample_state.alphaToCoverageEnable,
		multi_info.alphaToOneEnable = pipeline_configs[i].multisample_state.alphaToOneEnable;
		multi_infos.push_back(multi_info);

		//Depth stencil state
		VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {};
		depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_info.flags = pipeline_configs[i].depth_stencil_state.flags;
		depth_stencil_info.depthTestEnable = pipeline_configs[i].depth_stencil_state.depthTestEnable;
		depth_stencil_info.depthWriteEnable = pipeline_configs[i].depth_stencil_state.depthWriteEnable;
		depth_stencil_info.depthCompareOp = pipeline_configs[i].depth_stencil_state.depthCompareOp;
		depth_stencil_info.depthBoundsTestEnable = pipeline_configs[i].depth_stencil_state.depthBoundsTestEnable;
		depth_stencil_info.stencilTestEnable = pipeline_configs[i].depth_stencil_state.stencilTestEnable;
		depth_stencil_info.front = pipeline_configs[i].depth_stencil_state.front;
		depth_stencil_info.back = pipeline_configs[i].depth_stencil_state.back;
		depth_stencil_info.minDepthBounds = pipeline_configs[i].depth_stencil_state.minDepthBounds;
		depth_stencil_info.maxDepthBounds = pipeline_configs[i].depth_stencil_state.maxDepthBounds;
		ds_infos.push_back(depth_stencil_info);

		uint32_t current_attachment_count = pipeline_configs[i].blend_state.attachmentCount;
		for (uint32_t j = 0; j < current_attachment_count; j++) {
			const VulkanColorBlendAttachmentState state = pipeline_configs[i].blend_state.pAttachments[j];

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
		blend_info.flags = pipeline_configs[i].blend_state.flags;
		blend_info.logicOpEnable = pipeline_configs[i].blend_state.logicOpEnable;
		blend_info.logicOp = pipeline_configs[i].blend_state.logicOp;
		blend_info.attachmentCount = current_attachment_count;
		blend_info.pAttachments = blend_attachment_state_ptr;
		blend_info.blendConstants[0] = pipeline_configs[i].blend_state.blendConstants[0];
		blend_info.blendConstants[1] = pipeline_configs[i].blend_state.blendConstants[1];
		blend_info.blendConstants[2] = pipeline_configs[i].blend_state.blendConstants[2];
		blend_info.blendConstants[3] = pipeline_configs[i].blend_state.blendConstants[3];
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
		info.layout = _pipeline_layout;
		info.renderPass = *_render_passes.get(pipeline_configs[i].render_pass);
		info.subpass = 0;
		pipeline_infos.push_back(info);
	}

	std::vector<VkPipeline> pipelines;
	pipelines.resize(pipeline_configs.size());

	if (vkCreateGraphicsPipelines(device, pipeline_cache, (uint32_t)pipeline_configs.size(), pipeline_infos.data(), alloc_callbacks, pipelines.data()) != VK_SUCCESS) {
		printf("Creating graphics pipeline.\n");
		exit(-1);
	}

	for (uint32_t i = 0; i < pipeline_configs.size(); i++) {
		out_pipelines_handles[i] = _graphics_pipelines.insert({
			.pipeline = pipelines[i]
		});

		vkDestroyShaderModule(device, shader_stage_infos[2 * i].module, alloc_callbacks);
		vkDestroyShaderModule(device, shader_stage_infos[2 * i + 1].module, alloc_callbacks);
	}
}

Key<VulkanBuffer> VulkanGraphicsDevice::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage_flags, VmaAllocationCreateInfo& allocation_info) {
	VkBufferCreateInfo buffer_info = {};
	buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_info.size = size;
	buffer_info.usage = usage_flags;
	buffer_info.sharingMode = VK_SHARING_MODE_CONCURRENT;			//TODO: Trying out making all buffers concurrent to see what happens

	uint32_t queue_indices[] = {0, 0, 0};
	uint32_t queue_count = 1;
	if (compute_queue_family_idx != graphics_queue_family_idx) {
		queue_indices[1] = compute_queue_family_idx;
		queue_count += 1;
	}
	if (transfer_queue_family_idx != compute_queue_family_idx) {
		queue_indices[2] = transfer_queue_family_idx;
		queue_count += 1;
	}
	buffer_info.queueFamilyIndexCount = queue_count;
	buffer_info.pQueueFamilyIndices = queue_indices;

	VulkanBuffer buffer;
	if (vmaCreateBuffer(allocator, &buffer_info, &allocation_info, &buffer.buffer, &buffer.allocation, &buffer.alloc_info) != VK_SUCCESS) {
		printf("Creating buffer failed.\n");
		exit(-1);
	}

	return _buffers.insert(buffer);
}

VulkanBuffer* VulkanGraphicsDevice::get_buffer(Key<VulkanBuffer> key) {
	return _buffers.get(key);
}

VkDeviceAddress VulkanGraphicsDevice::buffer_device_address(Key<VulkanBuffer> key) {
	VkBufferDeviceAddressInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	
	info.buffer = _buffers.get(key)->buffer;
	return vkGetBufferDeviceAddress(device, &info);
}

void VulkanGraphicsDevice::destroy_buffer(Key<VulkanBuffer> key) {
	VulkanBuffer* b = _buffers.get(key);
	if (b) {
		BufferDeletion d = {
			.idx = EXTRACT_IDX(key.value()),
			.frames_til = FRAMES_IN_FLIGHT,
			.buffer = b->buffer,
			.allocation = b->allocation
		};
		_buffer_deletion_queue.emplace_front(d);
	}
}

VkSampler VulkanGraphicsDevice::create_sampler(VkSamplerCreateInfo& info) {
	VkSampler sampler;
	if (vkCreateSampler(device, &info, alloc_callbacks, &sampler) != VK_SUCCESS) {
		printf("Creating sampler failed.\n");
		exit(-1);
	}
	return sampler;
}

VkResult VulkanGraphicsDevice::create_images(std::span<VkImageCreateInfo> create_infos, Key<VulkanBindlessImage>* out_images) {
	for (uint32_t i = 0; i < create_infos.size(); ++i) {
		VkImageCreateInfo& info = create_infos[i];
		VulkanBindlessImage out_image = {};
		out_image.batch_id = 0;
		out_image.original_idx = 0;
		out_image.vk_image.width = info.extent.width;
		out_image.vk_image.height = info.extent.height;

		VmaAllocationCreateInfo alloc_info = {};
		alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
		alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		alloc_info.priority = 1.0;
		VKASSERT_OR_CRASH(vmaCreateImage(allocator, &info, &alloc_info, &out_image.vk_image.image, &out_image.vk_image.image_allocation, nullptr));

		VkImageAspectFlags image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
		if (
			info.format == VK_FORMAT_D16_UNORM ||
			info.format == VK_FORMAT_D16_UNORM_S8_UINT ||
			info.format == VK_FORMAT_D24_UNORM_S8_UINT ||
			info.format == VK_FORMAT_D32_SFLOAT ||
			info.format == VK_FORMAT_D32_SFLOAT_S8_UINT
		) image_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

		VkImageSubresourceRange subresource_range = {};
		subresource_range.aspectMask = image_aspect;
		subresource_range.baseMipLevel = 0;
		subresource_range.levelCount = info.mipLevels;
		subresource_range.baseArrayLayer = 0;
		subresource_range.layerCount = info.arrayLayers;

		VkImageViewCreateInfo view_info = {};
		view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_info.image = out_image.vk_image.image;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = info.format;
		view_info.components = COMPONENT_MAPPING_DEFAULT;
		view_info.subresourceRange = subresource_range;

		VKASSERT_OR_CRASH(vkCreateImageView(device, &view_info, alloc_callbacks, &out_image.vk_image.image_view));

		out_images[i] = bindless_images.insert(out_image);
	}

	return VK_SUCCESS;
}

void VulkanGraphicsDevice::submit_image_upload_batch(uint64_t id, const std::vector<RawImage>& raw_images, const std::vector<VkFormat>& image_formats) {
	VulkanImageUploadBatch current_batch = {};
	current_batch.id = id;

	uint32_t image_count = (uint32_t)raw_images.size();
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
	{
		VmaAllocationCreateInfo alloc_info = {};
		alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
		alloc_info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		alloc_info.priority = 1.0;
		current_batch.staging_buffer_id = create_buffer(total_staging_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, alloc_info);
	}
	VulkanBuffer* staging_buffer = _buffers.get(current_batch.staging_buffer_id);

	//Copy image data to staging buffer
	{
		uint8_t* mapped_head = static_cast<uint8_t*>(staging_buffer->alloc_info.pMappedData);

		for (uint32_t i = 0; i < image_count; i++) {
			void* image_bytes = raw_images[i].data;
			int x = raw_images[i].width;
			int y = raw_images[i].height;
			size_t num_bytes = static_cast<size_t>(x * y * channels);

			memcpy(mapped_head, image_bytes, num_bytes);

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

			vkCmdCopyBufferToImage(current_batch.command_buffer, staging_buffer->buffer, images[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		//Queue ownership transfer
		for (uint32_t i = 0; i < image_count; i++) {
			std::vector<VkImageMemoryBarrier2KHR> barriers;
			barriers.push_back({
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
			});

			if (mip_counts[i] > 1) {
				barriers.push_back({
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
				});
			}

			VkDependencyInfoKHR info = {};
			info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			info.imageMemoryBarrierCount = (uint32_t)barriers.size();
			info.pImageMemoryBarriers = barriers.data();

			vkCmdPipelineBarrier2KHR(current_batch.command_buffer, &info);
		}

		vkEndCommandBuffer(current_batch.command_buffer);
	}

	_pending_image_mutex.lock();
	_image_upload_mutex.lock();
	//Insert into pending images table
	for (uint32_t i = 0; i < image_count; i++) {
		VulkanPendingImage pending_image = {};
		pending_image.vk_image.image = images[i];
		pending_image.vk_image.image_view = image_views[i];
		pending_image.vk_image.image_allocation = image_allocations[i];
		pending_image.batch_id = current_batch.id;
		pending_image.vk_image.mip_levels = mip_counts[i];
		pending_image.vk_image.width = raw_images[i].width;
		pending_image.vk_image.height = raw_images[i].height;
		pending_image.vk_image.depth = 1;
		pending_image.original_idx = i;

		_pending_images.insert(pending_image);
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
		info.pSignalSemaphores = get_semaphore(image_upload_semaphore);
		info.commandBufferCount = 1;
		info.pCommandBuffers = &current_batch.command_buffer;
		info.pWaitDstStageMask = flags;

		VKASSERT_OR_CRASH(vkQueueSubmit(q, 1, &info, VK_NULL_HANDLE));

		_image_upload_batches.insert(current_batch);
	}
	_pending_image_mutex.unlock();
	_image_upload_mutex.unlock();
	
	printf("[image thread] Submitted batch #%i\n", (int)id);
}

uint64_t VulkanGraphicsDevice::load_raw_images(
	const std::vector<RawImage> raw_images,
	const std::vector<VkFormat> image_formats
) {
	_image_batches_requested += 1;
	_raw_image_mutex.lock();
	_raw_image_batch_queue.push({
		.id = _image_batches_requested,
		.raw_images = raw_images,
		.image_formats = image_formats
	});
	_raw_image_mutex.unlock();

	return _image_batches_requested;
}

uint64_t VulkanGraphicsDevice::load_compressed_images(
	const std::vector<CompressedImage> images,
	const std::vector<VkFormat> formats
) {
	_image_batches_requested += 1;
	_compressed_image_mutex.lock();
	_compressed_image_batch_queue.push({
		.id = _image_batches_requested,
		.images = images,
		.image_formats = formats
	});
	_compressed_image_mutex.unlock();
	
	return _image_batches_requested;
}

uint64_t VulkanGraphicsDevice::load_image_files(
	const std::vector<const char*> filenames,
	const std::vector<VkFormat> image_formats
) {
	_image_batches_requested += 1;
	_file_batch_mutex.lock();
	_image_file_batch_queue.push({
		.id = _image_batches_requested,
		.filenames = filenames,
		.image_formats = image_formats
	});
	_file_batch_mutex.unlock();

	return _image_batches_requested;
}

//Main function for image loading thread
void VulkanGraphicsDevice::load_images_impl() {
	while (_image_upload_running) {
		std::vector<RawImageBatchParameters> final_parameters;

		//Loading images from memory
		while (_raw_image_batch_queue.size() > 0) {
			RawImageBatchParameters params = _raw_image_batch_queue.front();

			final_parameters.push_back(params);

			//Remove processed parameters
			_raw_image_mutex.lock();
			_raw_image_batch_queue.pop();
			_raw_image_mutex.unlock();
		}

		//Loading compressed images from memory
		while (_compressed_image_batch_queue.size() > 0) {
			CompressedImageBatchParameters& params = _compressed_image_batch_queue.front();
			uint32_t image_count = (uint32_t)params.images.size();

			std::vector<RawImage> raw_images;
			raw_images.resize(image_count);
			for (uint32_t i = 0; i < image_count; i++) {
				int width, height;
				raw_images[i].data = stbi_load_from_memory(params.images[i].bytes.data(), static_cast<int>(params.images[i].bytes.size()), &width, &height, nullptr, STBI_rgb_alpha);
				raw_images[i].width = width;
				raw_images[i].height = height;
				printf("Decompressed image from memory with dimensions (%i, %i)\n", width, height);
			}

			RawImageBatchParameters p = {
				.id = params.id,
				.raw_images = raw_images,
				.image_formats = params.image_formats
			};
			final_parameters.push_back(p);

			//Free loaded image memory
			// for (RawImage& image : raw_images) {
			// 	stbi_image_free(image.data);
			// }

			_compressed_image_mutex.lock();
			_compressed_image_batch_queue.pop();
			_compressed_image_mutex.unlock();
		}

		//Loading images from files
		while (_image_file_batch_queue.size() > 0) {
			FileImageBatchParameters& params = _image_file_batch_queue.front();
			uint32_t image_count = (uint32_t)params.filenames.size();

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

			RawImageBatchParameters p = {
				.id = params.id,
				.raw_images = raw_images,
				.image_formats = params.image_formats
			};
			final_parameters.push_back(p);

			//Free loaded image memory
			// for (RawImage& image : raw_images) {
			// 	stbi_image_free(image.data);
			// }

			//Remove processed parameters
			_file_batch_mutex.lock();
			_image_file_batch_queue.pop();
			_file_batch_mutex.unlock();
		}

		//Call submit after batches have been collated
		std::sort(final_parameters.begin(), final_parameters.end(), [](RawImageBatchParameters& p1, RawImageBatchParameters& p2) {
			return p1.id < p2.id;
		});
		for (RawImageBatchParameters& p : final_parameters) {
			this->submit_image_upload_batch(p.id, p.raw_images, p.image_formats);
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
	// {
	// 	static int last_seen = 0;
	// 	if (gpu_batches_processed > last_seen) {
	// 		last_seen = (int)gpu_batches_processed;
	// 		printf("Saw batch %i with upload batch count of %i...\n", (int)gpu_batches_processed, (int)_image_upload_batches.count());
	// 	}
	// }

	//Descriptor update state
	std::vector<VkDescriptorImageInfo> desc_infos;
	std::vector<VkWriteDescriptorSet> desc_writes;
	desc_infos.reserve(16);
	desc_writes.reserve(16);

	std::vector<uint32_t> pending_images_to_delete;
	std::vector<uint32_t> batches_to_delete;
	for (auto batch_it = _image_upload_batches.begin(); batch_it != _image_upload_batches.end(); ++batch_it) {
		VulkanImageUploadBatch& batch = *batch_it;
		if (batch.id > gpu_batches_processed) continue;		

		//Make the transfer command buffer available again and destroy the staging buffer
		return_transfer_command_buffer(batch.command_buffer);
		vmaDestroyBuffer(allocator, _buffers.get(batch.staging_buffer_id)->buffer, _buffers.get(batch.staging_buffer_id)->allocation);

		for (auto pending_image_it = _pending_images.begin(); pending_image_it != _pending_images.end(); ++pending_image_it) {
			VulkanPendingImage& pending_image = *pending_image_it;

			//printf("pending_image.batch_id == %i\nbatch.id == %i\n", (int)pending_image.batch_id, (int)batch.id);
			if (pending_image.batch_id == batch.id) {
				//Record Graphics queue acquire ownership of the image
				{
					std::vector<VkImageMemoryBarrier2KHR> barriers;
					barriers.push_back({
						.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
						.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
						.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
						.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
						.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,

						.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						.srcQueueFamilyIndex = transfer_queue_family_idx,
						.dstQueueFamilyIndex = graphics_queue_family_idx,
						.image = pending_image.vk_image.image,
						.subresourceRange = {
							.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
							.baseMipLevel = 0,
							.levelCount = 1,
							.baseArrayLayer = 0,
							.layerCount = 1
						}
					});

					if (pending_image.vk_image.mip_levels > 1) {
						barriers.push_back({
							.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR,
							.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
							.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,
							.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR,
							.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT_KHR | VK_ACCESS_2_MEMORY_WRITE_BIT_KHR,

							.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
							.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							.srcQueueFamilyIndex = transfer_queue_family_idx,
							.dstQueueFamilyIndex = graphics_queue_family_idx,
							.image = pending_image.vk_image.image,
							.subresourceRange = {
								.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
								.baseMipLevel = 1,
								.levelCount = pending_image.vk_image.mip_levels - 1,
								.baseArrayLayer = 0,
								.layerCount = 1
							}
						});
					}

					VkDependencyInfoKHR info = {};
					info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
					info.imageMemoryBarrierCount = (uint32_t)barriers.size();
					info.pImageMemoryBarriers = barriers.data();

					vkCmdPipelineBarrier2KHR(render_cb, &info);
				}

				//Record mipmapping commands
				if (pending_image.vk_image.mip_levels > 1) {
					for (uint32_t k = 0; k < pending_image.vk_image.mip_levels - 1; k++) {
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
									.x = static_cast<int32_t>(pending_image.vk_image.width >> k),
									.y = static_cast<int32_t>(pending_image.vk_image.height >> k),
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
									.x = static_cast<int32_t>(pending_image.vk_image.width >> (k + 1)),
									.y = static_cast<int32_t>(pending_image.vk_image.height >> (k + 1)),
									.z = 1,
								}
							},
						};

						vkCmdBlitImage(
							render_cb,
							pending_image.vk_image.image,
							VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							pending_image.vk_image.image,
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
									.image = pending_image.vk_image.image,
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
									.image = pending_image.vk_image.image,
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
								.image = pending_image.vk_image.image,
								.subresourceRange = {
									.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
									.baseMipLevel = pending_image.vk_image.mip_levels - 1,
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
					VulkanBindlessImage ava = {};
					ava.batch_id = batch.id;
					ava.original_idx = pending_image.original_idx;
					ava.vk_image = pending_image.vk_image;
					
					Key<VulkanBindlessImage> handle = bindless_images.insert(ava);
					pending_images_to_delete.push_back(pending_image_it.slot_index());
					//printf("Pushed bindless image from batch %i into array\n", (int)batch.id);

					uint32_t descriptor_index = EXTRACT_IDX(handle.value());

					VkDescriptorImageInfo info = {
						.imageView = ava.vk_image.image_view,
						.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					};
					desc_infos.push_back(info);
					VkWriteDescriptorSet write = {
						.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
						.dstSet = _image_descriptor_set,
						.dstBinding = DescriptorBindings::SAMPLED_IMAGES,
						.dstArrayElement = descriptor_index,
						.descriptorCount = 1,
						.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
						.pImageInfo = &desc_infos[desc_infos.size() - 1]
					};
					desc_writes.push_back(write);
				}
			}
		}
		
		_image_batches_completed += 1;
		batches_to_delete.push_back(batch_it.slot_index());
	}
	
	for (uint32_t& idx : batches_to_delete) {
		_image_upload_batches.remove(idx);
	}

	//Remove completed pending images
	for (uint32_t& idx : pending_images_to_delete) {
		_pending_images.remove(idx);
	}

	//Release relevant locks
	_pending_image_mutex.unlock();
	_image_upload_mutex.unlock();
	
	if (batches_to_delete.size() > 0)
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(desc_writes.size()), desc_writes.data(), 0, nullptr);
}

uint64_t VulkanGraphicsDevice::completed_image_batches() {
	return _image_batches_completed;
}

void VulkanGraphicsDevice::destroy_image(Key<VulkanBindlessImage> key) {
	VulkanBindlessImage* im = bindless_images.get(key);
	if (im) {
		ImageDeletion d = {
			.idx = EXTRACT_IDX(key.value()),
			.frames_til = FRAMES_IN_FLIGHT,
			.image = im->vk_image.image,
			.image_view = im->vk_image.image_view,
			.image_allocation = im->vk_image.image_allocation
		};
		_image_deletion_queue.emplace_front(d);
	}
}

void VulkanGraphicsDevice::service_deletion_queues() {
	//Service buffer queue
	{
		uint32_t deleted_count = 0;
		for (BufferDeletion& d : _buffer_deletion_queue) {
			if (d.frames_til == 0) {
				vmaDestroyBuffer(allocator, d.buffer, d.allocation);
				_buffers.remove(d.idx);
				deleted_count += 1;
			} else {
				d.frames_til -= 1;
			}
		}

		while (deleted_count > 0) {
			_buffer_deletion_queue.pop_back();
			deleted_count -= 1;
		}
	}

	//Service image queue
	{
		uint32_t deleted_count = 0;
		for (ImageDeletion& d : _image_deletion_queue) {
			if (d.frames_til == 0) {
				vkDestroyImageView(device, d.image_view, alloc_callbacks);
				vmaDestroyImage(allocator, d.image, d.image_allocation);
				bindless_images.remove(d.idx);
				deleted_count += 1;
			} else {
				d.frames_til -= 1;
			}
		}

		while (deleted_count > 0) {
			_image_deletion_queue.pop_back();
			deleted_count -= 1;
		}
	}

	//Service command buffer queue
	{
		uint32_t deleted_count = 0;
		for (CommandBufferReturn& cb_return : _command_buffer_returns) {
			if (cb_return.wait_value <= check_timeline_semaphore(cb_return.wait_semaphore)) {
				_graphics_command_buffers.push(cb_return.cb);
				deleted_count += 1;
			}
		}

		while (deleted_count > 0) {
			_command_buffer_returns.pop_front();
			deleted_count -= 1;
		}
	}
}

Key<VkSemaphore> VulkanGraphicsDevice::create_semaphore(VkSemaphoreCreateInfo& info) {
	VkSemaphore s;
	if (vkCreateSemaphore(device, &info, alloc_callbacks, &s) != VK_SUCCESS) {
		printf("Creating semaphore failed.\n");
		exit(-1);
	}
	return _semaphores.insert(s);
}

VkSemaphore* VulkanGraphicsDevice::get_semaphore(Key<VkSemaphore> key) {
	return _semaphores.get(key);
}

Key<VkSemaphore> VulkanGraphicsDevice::create_timeline_semaphore(uint64_t initial_value) {
	VkSemaphoreTypeCreateInfo type_info = {};
	type_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	type_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	type_info.initialValue = initial_value;

	VkSemaphoreCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	info.pNext = &type_info;

	return create_semaphore(info);
}

uint64_t VulkanGraphicsDevice::check_timeline_semaphore(Key<VkSemaphore> semaphore) {
	uint64_t value;
	vkGetSemaphoreCounterValue(device, *get_semaphore(semaphore), &value);
	return value;
}

VulkanGraphicsPipeline* VulkanGraphicsDevice::get_graphics_pipeline(Key<VulkanGraphicsPipeline> handle) {
	return _graphics_pipelines.get(handle);
}

Key<VkFramebuffer> VulkanGraphicsDevice::create_framebuffer(VkFramebufferCreateInfo& info) {
	VkFramebuffer fb;
	if (vkCreateFramebuffer(device, &info, alloc_callbacks, &fb) != VK_SUCCESS) {
		printf("Creating framebuffer failed.\n");
		exit(-1);
	}
	return _framebuffers.insert(fb);
}

VkFramebuffer* VulkanGraphicsDevice::get_framebuffer(Key<VkFramebuffer> key) {
	return _framebuffers.get(key);
}

void VulkanGraphicsDevice::destroy_framebuffer(Key<VkFramebuffer> key) {
	_framebuffers.remove(EXTRACT_IDX(key.value()));
}

Key<VkRenderPass> VulkanGraphicsDevice::create_render_pass(VkRenderPassCreateInfo2& info) {
	VkRenderPass render_pass;	
	VKASSERT_OR_CRASH(vkCreateRenderPass2(device, &info, alloc_callbacks, &render_pass));
	return _render_passes.insert(render_pass);
}

VkRenderPass* VulkanGraphicsDevice::get_render_pass(Key<VkRenderPass> key) {
	return _render_passes.get(key);
}

VkPipelineLayout VulkanGraphicsDevice::get_pipeline_layout() {
	return _pipeline_layout;
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

void VulkanGraphicsDevice::begin_render_pass(VkCommandBuffer cb, VulkanFrameBuffer& fb) {
	VkRect2D area = {
		.offset = {
			.x = 0,
			.y = 0
		},
		.extent = {
			.width = fb.width,
			.height = fb.height
		}
	};

	VkRenderPassBeginInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.renderPass = *_render_passes.get(fb.render_pass);
	info.framebuffer = *_framebuffers.get(fb.fb);
	info.renderArea = area;
	info.clearValueCount = (uint32_t)fb.clear_values.size();
	info.pClearValues = fb.clear_values.data();

	vkCmdBeginRenderPass(cb, &info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanGraphicsDevice::end_render_pass(VkCommandBuffer cb) {
	vkCmdEndRenderPass(cb);
}