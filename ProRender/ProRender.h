// ProRender.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#define _CRT_SECURE_NO_WARNINGS

// TODO: Reference additional headers your program requires here.

#include <filesystem>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>
#include "SDL.h"
#include "SDL_main.h"
#include "SDL_vulkan.h"
#include "VulkanGraphicsDevice.h"

#ifdef WIN32
	#define VK_USE_PLATFORM_WIN32_KHR
#endif

#ifdef __linux__
	#define VK_USE_PLATFORM_XLIB_KHR
#endif

#define VOLK_IMPLEMENTATION
#include "volk.h"


struct Renderer {
	
};