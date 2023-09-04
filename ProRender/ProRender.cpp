// ProRender.cpp : Defines the entry point for the application.
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <vector>
#include "SDL.h"
#include "SDL_main.h"

#define VOLK_IMPLEMENTATION
#include "volk.h"

#include "ProRender.h"

int main(int argc, char* argv[]) {
	const VkAllocationCallbacks* alloc_callbacks = nullptr;

	SDL_Init(SDL_INIT_VIDEO);	//Initialize SDL

	const uint32_t x_resolution = 1280;
	const uint32_t y_resolution = 720;
	SDL_Window* window = SDL_CreateWindow("losing my mind", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, x_resolution, y_resolution, SDL_WINDOW_VULKAN);

	//Try to initialize volk
	if (volkInitialize() != VK_SUCCESS) {
		printf("RIP\n");
		exit(-1);
	}
	printf("Volk initialized.\n");

	//Vulkan instance creation
	VkInstance vulkan_instance;
	{
		VkApplicationInfo app_info;
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pNext = nullptr;
		app_info.pApplicationName = "ProRender";
		app_info.applicationVersion = 1;
		app_info.pEngineName = nullptr;
		app_info.engineVersion = 0;
		app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 2, 0);

		VkInstanceCreateInfo inst_info;
		inst_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		inst_info.pNext = nullptr;
		inst_info.flags = 0;
		inst_info.pApplicationInfo = &app_info;
		inst_info.enabledLayerCount = 0;
		inst_info.ppEnabledLayerNames = nullptr;
		inst_info.enabledExtensionCount = 0;
		inst_info.ppEnabledExtensionNames = nullptr;

		if (vkCreateInstance(&inst_info, alloc_callbacks, &vulkan_instance) != VK_SUCCESS) {
			printf("Instance creation failed.\n");
			exit(-1);
		}
		printf("Instance created.\n");
	}

	//Load all Vulkan entrypoints, including extensions
	//volkLoadInstance(vulkan_instance);
	volkLoadInstanceOnly(vulkan_instance);

	//Ok so I apparently don't want to do that and instead want to use volkLoadDevice() after my VkDevice has been created
	
	//Vulkan physical device selection
	VkPhysicalDevice chosen_device = 0;
	uint32_t graphics_queue_index, compute_queue_index, transfer_queue_index;
	{
		uint32_t physical_device_count = 0;
		//Getting physical device count by passing nullptr as last param
		if (vkEnumeratePhysicalDevices(vulkan_instance, &physical_device_count, nullptr) != VK_SUCCESS) {
			printf("Querying physical device count failed.\n");
			exit(-1);
		}
		printf("%i physical devices available.\n", physical_device_count);

		VkPhysicalDevice devices[16];
		if (vkEnumeratePhysicalDevices(vulkan_instance, &physical_device_count, devices) != VK_SUCCESS) {
			printf("Querying physical devices failed.\n");
			exit(-1);
		}

		const VkPhysicalDeviceType TYPES[] = {VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, VK_PHYSICAL_DEVICE_TYPE_CPU};
		for (uint32_t j = 0; j < 3; j++) {
			for (uint32_t i = 0; i < physical_device_count; i++) {
				VkPhysicalDevice device = devices[i];
				VkPhysicalDeviceProperties2 properties;
				properties.pNext = nullptr;
				properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
				vkGetPhysicalDeviceProperties2(device, &properties);

				uint32_t queue_count = 0;
				vkGetPhysicalDeviceQueueFamilyProperties2(device, &queue_count, nullptr);

				VkQueueFamilyProperties2 queue_properties[16];
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
						graphics_queue_index = k;
						compute_queue_index = k;
						transfer_queue_index = k;
						continue;
					}

					if (props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT == 0 &&
					    props.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT) {
						compute_queue_index = k;
						continue;
					}

					if (props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT == 0 &&
					    props.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT) {
						transfer_queue_index = k;
						continue;
					}
				}

				if (properties.properties.deviceType == TYPES[j]) {
					chosen_device = device;
					printf("Chosen physical device: %s\n", properties.properties.deviceName);
					break;
				}
			}
			if (chosen_device) break;
		}
	}

	//Vulkan device creation
	VkDevice vk_device;
	{
		std::vector<VkDeviceQueueCreateInfo> queue_infos;

		float priority = 1.0;
		VkDeviceQueueCreateInfo g_queue_info;
		g_queue_info.pNext = nullptr;
		g_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		g_queue_info.flags = 0;
		g_queue_info.queueFamilyIndex = graphics_queue_index;
		g_queue_info.queueCount = 1;
		g_queue_info.pQueuePriorities = &priority;
		queue_infos.push_back(g_queue_info);

		if (graphics_queue_index != compute_queue_index) {
			VkDeviceQueueCreateInfo c_queue_info;
			c_queue_info.pNext = nullptr;
			c_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			c_queue_info.flags = 0;
			c_queue_info.queueFamilyIndex = compute_queue_index;
			c_queue_info.queueCount = 1;
			c_queue_info.pQueuePriorities = &priority;
			queue_infos.push_back(c_queue_info);
		}

		if (graphics_queue_index != transfer_queue_index) {
			VkDeviceQueueCreateInfo t_queue_info;
			t_queue_info.pNext = nullptr;
			t_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			t_queue_info.flags = 0;
			t_queue_info.queueFamilyIndex = transfer_queue_index;
			t_queue_info.queueCount = 1;
			t_queue_info.pQueuePriorities = &priority;
			queue_infos.push_back(t_queue_info);
		}

		VkDeviceCreateInfo device_info;
		device_info.pNext = nullptr;
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.queueCreateInfoCount = queue_infos.size();
		device_info.pQueueCreateInfos = queue_infos.data();
		device_info.enabledLayerCount = 0;
		device_info.ppEnabledLayerNames = nullptr;
		device_info.enabledExtensionCount = 0;
		device_info.ppEnabledExtensionNames = nullptr;
		device_info.pEnabledFeatures = nullptr;

		if (vkCreateDevice(chosen_device, &device_info, alloc_callbacks, &vk_device) != VK_SUCCESS) {
			printf("Creating logical device failed.\n");
			exit(-1);			
		}
	}
	printf("Logical device created.\n");

	//Main loop
	bool running = true;
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
	}

	//Cleanup resources
	vkDestroyInstance(vulkan_instance, alloc_callbacks);
	SDL_Quit();

	return 0;
}
