// ProRender.cpp : Defines the entry point for the application.
//

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
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
	volkLoadInstance(vulkan_instance);
	//volkLoadInstanceOnly(vulkan_instance);

	//Ok so I apparently don't want to do that and instead want to use volkLoadDevice() after my VkDevice has been created
	
	//Vulkan device creation
	{
		uint32_t physical_device_count = 0;
		//Getting physical device count by passing nullptr as last param
		if (vkEnumeratePhysicalDevices(vulkan_instance, &physical_device_count, nullptr) != VK_SUCCESS) {
			printf("Querying physical device count failed.\n");
			exit(-1);
		}
		printf("%i physical devices available.\n", physical_device_count);

		VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(physical_device_count * sizeof(VkPhysicalDevice));
		if (vkEnumeratePhysicalDevices(vulkan_instance, &physical_device_count, devices) != VK_SUCCESS) {
			printf("Querying physical devices failed.\n");
			exit(-1);
		}

		for (uint32_t i = 0; i < physical_device_count; i++) {
			printf("i = %i\n", i);
			VkPhysicalDevice& device = devices[i];
			VkPhysicalDeviceProperties2 properties;
			properties.pNext = nullptr;
			properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			vkGetPhysicalDeviceProperties2(device, &properties);

			printf("%s\n", properties.properties.deviceName);
		}
	}

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
