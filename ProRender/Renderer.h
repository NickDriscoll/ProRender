#include <stdint.h>
#include "VulkanGraphicsDevice.h"

struct Renderer {
	VulkanGraphicsDevice* vgd;		//Very dangerous and dubiously recommended

	uint64_t imgui_vertex_buffer;
	uint64_t imgui_index_buffer;

	Renderer(VulkanGraphicsDevice* vgd);
	~Renderer();
};