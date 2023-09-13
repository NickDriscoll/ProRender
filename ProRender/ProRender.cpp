// ProRender.cpp : Defines the entry point for the application.
//

#include "ProRender.h"

void initVulkanGraphicsDevice(VulkanGraphicsDevice& vgd) {
	//Assumptions straight from my butt
	const uint32_t MAX_PHYSICAL_DEVICES = 16;
	const uint32_t MAX_DEVICE_QUEUES = 16;

	vgd.alloc_callbacks = nullptr;

	//Try to initialize volk
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
#ifdef WIN32
		extension_names.push_back(static_cast<const char*>("VK_KHR_win32_surface"));
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

		if (vkCreateInstance(&inst_info, vgd.alloc_callbacks, &vgd.instance) != VK_SUCCESS) {
			printf("Instance creation failed.\n");
			exit(-1);
		}
		printf("Instance created.\n");
	}

	//Load all Vulkan instance entrypoints
	volkLoadInstanceOnly(vgd.instance);

	//Vulkan physical device selection
	{
		uint32_t physical_device_count = 0;
		//Getting physical device count by passing nullptr as last param
		if (vkEnumeratePhysicalDevices(vgd.instance, &physical_device_count, nullptr) != VK_SUCCESS) {
			printf("Querying physical device count failed.\n");
			exit(-1);
		}
		printf("%i physical devices available.\n", physical_device_count);

		VkPhysicalDevice devices[MAX_PHYSICAL_DEVICES];
		if (vkEnumeratePhysicalDevices(vgd.instance, &physical_device_count, devices) != VK_SUCCESS) {
			printf("Querying physical devices failed.\n");
			exit(-1);
		}

		const VkPhysicalDeviceType TYPES[] = { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, VK_PHYSICAL_DEVICE_TYPE_CPU };
		for (uint32_t j = 0; j < 3; j++) {
			for (uint32_t i = 0; i < physical_device_count; i++) {
				VkPhysicalDevice device = devices[i];
				VkPhysicalDeviceProperties2 properties;
				properties.pNext = nullptr;
				properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				vkGetPhysicalDeviceProperties2(device, &properties);

				uint32_t queue_count = 0;
				vkGetPhysicalDeviceQueueFamilyProperties2(device, &queue_count, nullptr);

				VkQueueFamilyProperties2 queue_properties[MAX_DEVICE_QUEUES];
				//C++ initialization is hell
				for (uint32_t k = 0; k < queue_count; k++) {
					queue_properties[k].pNext = nullptr;
					queue_properties[k].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
				}

				vkGetPhysicalDeviceQueueFamilyProperties2(device, &queue_count, queue_properties);

				//Check for compute and transfer queues
				for (uint32_t k = 0; k < queue_count; k++) {
					VkQueueFamilyProperties2& props = queue_properties[k];
					if (props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
						vgd.graphics_queue = k;
						vgd.compute_queue = k;
						vgd.transfer_queue = k;
						continue;
					}

					if ((props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
						props.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
						vgd.compute_queue = k;
						continue;
					}

					if ((props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
						props.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) {
						vgd.transfer_queue = k;
						continue;
					}
				}

				if (properties.properties.deviceType == TYPES[j]) {
					vgd.physical_device = device;
					printf("Chosen physical device: %s\n", properties.properties.deviceName);
					break;
				}
			}
			if (vgd.physical_device) break;
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
		g_queue_info.queueFamilyIndex = vgd.graphics_queue;
		g_queue_info.queueCount = 1;
		g_queue_info.pQueuePriorities = &priority;
		queue_infos.push_back(g_queue_info);

		//If we have a dedicated compute queue family
		if (vgd.graphics_queue != vgd.compute_queue) {
			VkDeviceQueueCreateInfo c_queue_info;
			c_queue_info.pNext = nullptr;
			c_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			c_queue_info.flags = 0;
			c_queue_info.queueFamilyIndex = vgd.compute_queue;
			c_queue_info.queueCount = 1;
			c_queue_info.pQueuePriorities = &priority;
			queue_infos.push_back(c_queue_info);
		}

		//If we have a dedicated transfer queue family
		if (vgd.graphics_queue != vgd.transfer_queue) {
			VkDeviceQueueCreateInfo t_queue_info;
			t_queue_info.pNext = nullptr;
			t_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			t_queue_info.flags = 0;
			t_queue_info.queueFamilyIndex = vgd.transfer_queue;
			t_queue_info.queueCount = 1;
			t_queue_info.pQueuePriorities = &priority;
			queue_infos.push_back(t_queue_info);
		}

		std::vector<const char*> extension_names = {"VK_KHR_swapchain"};

		VkDeviceCreateInfo device_info;
		device_info.pNext = nullptr;
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.flags = 0;
		device_info.queueCreateInfoCount = queue_infos.size();
		device_info.pQueueCreateInfos = queue_infos.data();
		device_info.enabledLayerCount = 0;
		device_info.ppEnabledLayerNames = nullptr;
		device_info.enabledExtensionCount = extension_names.size();
		device_info.ppEnabledExtensionNames = extension_names.data();
		device_info.pEnabledFeatures = nullptr;

		if (vkCreateDevice(vgd.physical_device, &device_info, vgd.alloc_callbacks, &vgd.device) != VK_SUCCESS) {
			printf("Creating logical device failed.\n");
			exit(-1);
		}
	}
	printf("Logical device created.\n");

	volkLoadDevice(vgd.device);

	//Create command pool
	{
		VkCommandPoolCreateInfo pool_info;
		pool_info.pNext = nullptr;
		pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pool_info.queueFamilyIndex = vgd.graphics_queue;

		if (vkCreateCommandPool(vgd.device, &pool_info, vgd.alloc_callbacks, &vgd.command_pool) != VK_SUCCESS) {
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
		cb_info.commandPool = vgd.command_pool;
		cb_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cb_info.commandBufferCount = FRAMES_IN_FLIGHT;

		if (vkAllocateCommandBuffers(vgd.device, &cb_info, vgd.command_buffers) != VK_SUCCESS) {
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
		if (vkCreatePipelineCache(vgd.device, &info, vgd.alloc_callbacks, &vgd.pipeline_cache) != VK_SUCCESS) {
			printf("Creating pipeline cache failed.\n");
			exit(-1);
		}
	}
	printf("Created pipeline cache.\n");

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

VkShaderModule load_shader_module(VulkanGraphicsDevice& vgd, const char* path) {
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
	if (vkCreateShaderModule(vgd.device, &info, vgd.alloc_callbacks, &shader) != VK_SUCCESS) {
		printf("Creating vertex shader module failed.\n");
		exit(-1);
	}

	return shader;
}

int main(int argc, char* argv[]) {
	SDL_Init(SDL_INIT_VIDEO);	//Initialize SDL

	const uint32_t x_resolution = 1024;
	const uint32_t y_resolution = 1024;
	SDL_Window* window = SDL_CreateWindow("losing my mind", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, x_resolution, y_resolution, SDL_WINDOW_VULKAN);

	//Init vulkan graphics device
	VulkanGraphicsDevice vgd;
	initVulkanGraphicsDevice(vgd);

	//VkSurface and swapchain creation
	VkFormat preferred_swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;	//This seems to be a pretty standard/common swapchain format
	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	{
		if (SDL_Vulkan_CreateSurface(window, vgd.instance, &surface) == SDL_FALSE) {
			printf("Creating VkSurface failed.\n");
			exit(-1);
		}
		printf("Created surface.\n");

		//Query for surface capabilities
		VkSurfaceCapabilitiesKHR surface_capabilities = {};
		if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vgd.physical_device, surface, &surface_capabilities) != VK_SUCCESS) {
			printf("Getting VkSurface capabilities failed.\n");
			exit(-1);
		}

		uint32_t format_count;
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(vgd.physical_device, surface, &format_count, nullptr) != VK_SUCCESS) {
			printf("Getting surface format count failed.\n");
			exit(-1);
		}

		std::vector<VkSurfaceFormatKHR> formats;
		formats.resize(format_count);
		if (vkGetPhysicalDeviceSurfaceFormatsKHR(vgd.physical_device, surface, &format_count, formats.data()) != VK_SUCCESS) {
			printf("Getting surface formats failed.\n");
			exit(-1);
		}

		bool found_preferred_format = false;
		for (uint32_t i = 0; i < format_count; i++) {
			VkFormat format = formats[i].format;
			if (format == preferred_swapchain_format) {
				found_preferred_format = true;
				break;
			}
		}
		if (!found_preferred_format) {
			printf("Surface doesn't support VK_FORMAT_B8G8R8A8_SRGB.\n");
			exit(-1);
		}

		uint32_t present_mode_count;
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(vgd.physical_device, surface, &present_mode_count, nullptr) != VK_SUCCESS) {
			printf("Getting present modes count failed.\n");
			exit(-1);
		}

		std::vector<VkPresentModeKHR> present_modes;
		present_modes.resize(present_mode_count);
		if (vkGetPhysicalDeviceSurfacePresentModesKHR(vgd.physical_device, surface, &present_mode_count, present_modes.data()) != VK_SUCCESS) {
			printf("Getting present modes failed.\n");
			exit(-1);
		}

		for (uint32_t i = 0; i < present_mode_count; i++) {
			VkPresentModeKHR mode = present_modes[i];

		}

		VkSwapchainCreateInfoKHR swapchain_info = {};
		swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_info.surface = surface;
		swapchain_info.minImageCount = surface_capabilities.minImageCount;
		swapchain_info.imageFormat = preferred_swapchain_format;
		swapchain_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		swapchain_info.imageExtent = surface_capabilities.currentExtent;
		swapchain_info.imageArrayLayers = 1;
		swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_info.queueFamilyIndexCount = 1;
		swapchain_info.pQueueFamilyIndices = &vgd.graphics_queue;
		swapchain_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		swapchain_info.clipped = VK_TRUE;

		if (vkCreateSwapchainKHR(vgd.device, &swapchain_info, vgd.alloc_callbacks, &swapchain) != VK_SUCCESS) {
			printf("Swapchain creation failed.\n");
			exit(-1);
		}
		printf("Created swapchain.\n");
	}

	//Create pipeline
	VkDescriptorSetLayout descriptor_set_layout;
	VkPipelineLayout pipeline_layout;
	VkRenderPass render_pass;
	VkPipeline main_pipeline;
	{
		//Shader stages
		VkShaderModule vertex_shader = load_shader_module(vgd, "shaders/test.vert.spv");
		VkShaderModule fragment_shader = load_shader_module(vgd, "shaders/test.frag.spv");
		std::vector<VkPipelineShaderStageCreateInfo> shader_stage_creates;
		{

			VkPipelineShaderStageCreateInfo vert_info = {};
			vert_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vert_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vert_info.module = vertex_shader;
			vert_info.pName = "main";
			shader_stage_creates.push_back(vert_info);

			VkPipelineShaderStageCreateInfo frag_info = {};
			frag_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			frag_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			frag_info.module = fragment_shader;
			frag_info.pName = "main";
			shader_stage_creates.push_back(frag_info);
		}

		//Vertex input state
		//Doing vertex pulling so this can be mostly null :)
		VkPipelineVertexInputStateCreateInfo vert_input_info = {};
		vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;


		//Input assembly state
		VkPipelineInputAssemblyStateCreateInfo ia_info = {};
		ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		//Tesselation state
		VkPipelineTessellationStateCreateInfo tess_info = {};
		tess_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;

		//Viewport and scissor state
		//These will _always_ be dynamic pipeline states
		VkPipelineViewportStateCreateInfo viewport_info = {};
		viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_info.viewportCount = 1;
		viewport_info.scissorCount = 1;

		//Rasterization state info
		VkPipelineRasterizationStateCreateInfo rast_info = {};
		rast_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rast_info.depthClampEnable = VK_FALSE;
		rast_info.rasterizerDiscardEnable = VK_FALSE;
		rast_info.polygonMode = VK_POLYGON_MODE_FILL;
		rast_info.cullMode = VK_CULL_MODE_BACK_BIT;
		rast_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rast_info.depthBiasEnable = VK_FALSE;
		rast_info.lineWidth = 1.0;

		//Multisample state
		VkPipelineMultisampleStateCreateInfo multi_info = {};
		multi_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multi_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multi_info.sampleShadingEnable = VK_FALSE;

		//Depth stencil state
		VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {};
		depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_info.depthTestEnable = VK_TRUE;
		depth_stencil_info.depthWriteEnable = VK_TRUE;
		depth_stencil_info.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
		depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_info.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo blend_info = {};
		{
			//Blend func description
			VkColorComponentFlags component_flags = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
			VkPipelineColorBlendAttachmentState blend_state = {};
			blend_state.blendEnable = VK_TRUE;
			blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
			blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
			blend_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			blend_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
			blend_state.alphaBlendOp = VK_BLEND_OP_ADD;
			blend_state.colorWriteMask = component_flags;


			//Blend state
			blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			blend_info.attachmentCount = 1;
			blend_info.pAttachments = &blend_state;
			blend_info.blendConstants[0] = 1.0;
			blend_info.blendConstants[1] = 1.0;
			blend_info.blendConstants[2] = 1.0;
			blend_info.blendConstants[3] = 1.0;
		}

		//Dynamic state info
		VkDynamicState dyn_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
		VkPipelineDynamicStateCreateInfo dynamic_info = {};
		dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_info.dynamicStateCount = 2;
		dynamic_info.pDynamicStates = dyn_states;

		//Pipeline layout
		{
			//Descriptor set layout
			{
				VkDescriptorSetLayoutCreateInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;


				if (vkCreateDescriptorSetLayout(vgd.device, &info, vgd.alloc_callbacks, &descriptor_set_layout) != VK_SUCCESS) {
					printf("Creating descriptor set layout failed.\n");
					exit(-1);
				}
			}

			VkPipelineLayoutCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			info.setLayoutCount = 1;
			info.pSetLayouts = &descriptor_set_layout;
			info.pushConstantRangeCount = 0;


			if (vkCreatePipelineLayout(vgd.device, &info, vgd.alloc_callbacks, &pipeline_layout) != VK_SUCCESS) {
				printf("Creating pipeline layout failed.\n");
				exit(-1);
			}
		}
		printf("Created pipeline layout.\n");

		//Render pass object
		{
			VkAttachmentDescription color_attachment = {
				.format = preferred_swapchain_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
			};

			VkAttachmentReference attachment_ref = {
				.attachment = 0,
				.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			};

			VkSubpassDescription subpass = {
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = 1,
				.pColorAttachments = &attachment_ref
			};

			VkRenderPassCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			info.attachmentCount = 1;
			info.pAttachments = &color_attachment;
			info.subpassCount = 1;
			info.pSubpasses = &subpass;


			if (vkCreateRenderPass(vgd.device, &info, vgd.alloc_callbacks, &render_pass) != VK_SUCCESS) {
				printf("Creating render pass failed.\n");
				exit(-1);
			}
		}

		VkGraphicsPipelineCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		info.stageCount = shader_stage_creates.size();
		info.pStages = shader_stage_creates.data();
		info.pVertexInputState = &vert_input_info;
		info.pInputAssemblyState = &ia_info;
		info.pTessellationState = &tess_info;
		info.pViewportState = &viewport_info;
		info.pRasterizationState = &rast_info;
		info.pMultisampleState = &multi_info;
		info.pDepthStencilState = &depth_stencil_info;
		info.pColorBlendState = &blend_info;
		info.pDynamicState = &dynamic_info;
		info.layout = pipeline_layout;
		info.renderPass = render_pass;
		info.subpass = 0;


		if (vkCreateGraphicsPipelines(vgd.device, vgd.pipeline_cache, 1, &info, vgd.alloc_callbacks, &main_pipeline) != VK_SUCCESS) {
			printf("Creating graphics pipeline.\n");
			exit(-1);
		}

		vkDestroyShaderModule(vgd.device, vertex_shader, vgd.alloc_callbacks);
		vkDestroyShaderModule(vgd.device, fragment_shader, vgd.alloc_callbacks);
	}
	printf("Created graphics pipeline.\n");

	//Main loop
	bool running = true;
	uint64_t current_frame = 0;
	while (running) {
		//Do input polling loop
		SDL_Event current_event;
		while (SDL_PollEvent(&current_event)) {
			switch (current_event.type) {
			case SDL_QUIT:
				running = false;
				break;
			}
		}

		//Draw triangle
		{
			VkCommandBuffer current_cb = vgd.command_buffers[current_frame % FRAMES_IN_FLIGHT];

			VkCommandBufferBeginInfo begin_info = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			vkBeginCommandBuffer(current_cb, &begin_info);



			vkEndCommandBuffer(current_cb);
		}
		current_frame++;
	}

	//Cleanup resources
	vkDestroySwapchainKHR(vgd.device, swapchain, vgd.alloc_callbacks);
	vkDestroySurfaceKHR(vgd.instance, surface, vgd.alloc_callbacks);
	vkDestroyCommandPool(vgd.device, vgd.command_pool, vgd.alloc_callbacks);
	vkDestroyPipeline(vgd.device, main_pipeline, vgd.alloc_callbacks);
	vkDestroyRenderPass(vgd.device, render_pass, vgd.alloc_callbacks);
	vkDestroyPipelineLayout(vgd.device, pipeline_layout, vgd.alloc_callbacks);
	vkDestroyDescriptorSetLayout(vgd.device, descriptor_set_layout, vgd.alloc_callbacks);
	vkDestroyPipelineCache(vgd.device, vgd.pipeline_cache, vgd.alloc_callbacks);
	vkDestroyDevice(vgd.device, vgd.alloc_callbacks);
	vkDestroyInstance(vgd.instance, vgd.alloc_callbacks);
	SDL_Quit();

	return 0;
}
