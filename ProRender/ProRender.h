#ifdef _WIN32
	#define VK_USE_PLATFORM_WIN32_KHR
	#define NOMINMAX
#endif
#ifdef __linux__
	#define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <bit>
#include <chrono>
#include <filesystem>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <thread>
#include <math.h>
#include <hlsl++.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <fastgltf/core.hpp>
#include "volk.h"
#include "stb_image.h"
#include "VulkanWindow.h"
#include "Renderer.h"
#include "vma.h"

struct Configuration {
	uint32_t window_width;
	uint32_t window_height;
};

struct Ps1Prop {
	
};