// ProRender.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#define _CRT_SECURE_NO_WARNINGS

// TODO: Reference additional headers your program requires here.

#ifdef _WIN32
	#define VK_USE_PLATFORM_WIN32_KHR
#endif

#ifdef __linux__
	#define VK_USE_PLATFORM_XLIB_KHR
#endif

#include <bit>
#include <chrono>
#include <filesystem>
#include <stdio.h>
//#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <thread>
#include <hlsl++.h>
#include "SDL.h"
#include "SDL_main.h"
#include "SDL_vulkan.h"
#include "VulkanWindow.h"
#include "volk.h"
#include "vma.h"
#include "stb_image.h"
#include <imgui.h>
#include "Renderer.h"

constexpr uint64_t U64_MAX = 0xFFFFFFFFFFFFFFFF;

struct Configuration {
	uint32_t window_width;
	uint32_t window_height;
};