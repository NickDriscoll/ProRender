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

int main(int argc, char* argv[]) {
	SDL_Init(SDL_INIT_VIDEO);	//Initialize SDL

	const uint32_t x_resolution = 1280;
	const uint32_t y_resolution = 720;
	SDL_Window* window = SDL_CreateWindow("losing my mind", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, x_resolution, y_resolution, SDL_WINDOW_VULKAN);

	//Init vulkan graphics device
	VulkanGraphicsDevice vgd;
	initVulkanGraphicsDevice(vgd);

	//VkSurface and swapchain creation
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

		VkFormat preferred_swapchain_format = VK_FORMAT_B8G8R8A8_SRGB;	//This seems to be a pretty standard/common swapchain format
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

	//Create 
	{

	}

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
	vkDestroyDevice(vgd.device, vgd.alloc_callbacks);
	vkDestroyInstance(vgd.instance, vgd.alloc_callbacks);
	SDL_Quit();

	return 0;
}
