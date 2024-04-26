#include <span>
#include <unordered_map>
#include <string.h>
#include <hlsl++.h>
#include <imgui.h>
#include "slotmap.h"
#include "VulkanGraphicsDevice.h"

#define MAX_CAMERAS 64
#define MAX_MATERIALS 1024
#define MAX_MATERIAL_TEXTURES 8
#define MAX_VERTEX_ATTRIBS 1024
#define MAX_MESHES 1024*1024
#define MAX_INDIRECT_DRAWS 100000
#define MAX_INSTANCES 1024*1024

enum DescriptorBindings : uint32_t {
	SAMPLED_IMAGES = 0,
	SAMPLERS = 1,
	FRAME_UNIFORMS = 2,
	IMGUI_POSITIONS = 3,
	IMGUI_UVS = 4,
	IMGUI_COLORS = 5,
	VERTEX_POSITIONS = 6,
	VERTEX_UVS = 7,
	CAMERA_BUFFER = 8,
	MESH_BUFFER = 9,
	MATERIAL_BUFFER = 10,
	INSTANCE_DATA_BUFFER = 11
};

struct FrameUniforms {
	hlslpp::float4x4 clip_from_screen;
};

struct Camera {
	hlslpp::float3 position;
	float yaw;
	float pitch;
	float roll;

	hlslpp::float4x4 make_view_matrix();
};

struct GPUCamera {
	hlslpp::float4x4 view_matrix;
	hlslpp::float4x4 projection_matrix;
};

struct GPUMesh {
	uint32_t position_start;
	uint32_t uv_start;
};

struct GPUMaterial {
	uint32_t texture_indices[MAX_MATERIAL_TEXTURES] = {std::numeric_limits<uint32_t>::max()};
	hlslpp::float4 base_color;
	uint32_t sampler_idx;
	uint32_t _pad0 = 0, _pad1 = 0;
};

struct GPUInstanceData {
	hlslpp::float4x4 world_matrix;
	uint32_t mesh_idx;
	uint32_t material_idx;
	uint32_t _pad0 = 0, _pad1 = 0;
};

struct BufferView {
	uint32_t start;
	uint32_t length;
};

struct MeshAttribute {
	Key<BufferView> position_key;
	BufferView view;
};

struct Material {
	hlslpp::float4 base_color;
	uint64_t batch_id;		//The batch where this material's textures come from
	uint32_t sampler_idx;
};

struct InstanceData {
	hlslpp::float4x4 world_from_model;
};

struct VulkanRenderer {
	Key<VulkanGraphicsPipeline> ps1_pipeline;

	Key<VkSemaphore> frames_completed_semaphore;

	Key<VkDescriptorSetLayout> descriptor_set_layout_id;
	Key<VkPipelineLayout> pipeline_layout_id;
	VkDescriptorPool descriptor_pool;
	VkDescriptorSet descriptor_set;

	//Buffer of per-frame uniform data
	FrameUniforms frame_uniforms;
	Key<VulkanBuffer> frame_uniforms_buffer;

	//Buffer of camera data
	Key<VulkanBuffer> camera_buffer;
	slotmap<Camera> cameras;

	//Vertex buffers
	uint32_t vertex_position_offset = 0;
	Key<VulkanBuffer> vertex_position_buffer;
	uint32_t vertex_uv_offset = 0;
	Key<VulkanBuffer> vertex_uv_buffer;

	//Buffer for storing all loaded mesh indices
	uint32_t index_buffer_offset = 0;
	Key<VulkanBuffer> index_buffer;

	Key<BufferView> push_vertex_positions(std::span<float> data);
	BufferView* get_vertex_positions(Key<BufferView> key);
	Key<MeshAttribute> push_vertex_uvs(Key<BufferView> position_key, std::span<float> data);
	BufferView* get_vertex_uvs(Key<BufferView> key);
	Key<MeshAttribute> push_indices16(Key<BufferView> position_key, std::span<uint16_t> data);
	BufferView* get_indices16(Key<BufferView> position_key);

	Key<Material> push_material(uint64_t batch_id, uint32_t sampler_idx, hlslpp::float4& base_color);
	
	uint32_t standard_sampler_idx;
	uint32_t point_sampler_idx;

	uint64_t get_current_frame();

	//Called during the main simulation whenever we want to draw something
	void ps1_draw(Key<BufferView> mesh_key, Key<Material> material_key, const std::span<InstanceData>& instance_datas);

	//Called at the end of each frame
	void render(VkCommandBuffer frame_cb, VulkanFrameBuffer& framebuffer, SyncData& sync_data);

	VulkanRenderer(VulkanGraphicsDevice* vgd, Key<VkRenderPass> swapchain_renderpass);
	~VulkanRenderer();

private:
	slotmap<BufferView> _position_buffers;
	slotmap<MeshAttribute> _uv_buffers;
	slotmap<MeshAttribute> _index16_buffers;
	slotmap<Material> _materials;
	std::vector<VkSampler> _samplers;

	//Draw stream state
	Key<VulkanBuffer> _indirect_draw_buffer;
	std::vector<VkDrawIndexedIndirectCommand> _draw_calls;	//Reset every frame
	uint32_t _instances_so_far = 0;

	Key<VulkanBuffer> _instance_buffer;
	std::vector<GPUInstanceData> _gpu_instance_datas;

	Key<VulkanBuffer> _mesh_buffer;
	slotmap<GPUMesh> _gpu_meshes;
	std::unordered_map<uint64_t, uint64_t> _mesh_map;
	bool _mesh_dirty_flag = false;

	//Reference to GPU buffer of GPUMaterial structs
	Key<VulkanBuffer> _material_buffer;
	slotmap<GPUMaterial> _gpu_materials;
	//std::unordered_map<Key<Material>, Key<GPUMaterial>> _material_map;
	std::unordered_map<uint64_t, uint64_t> _material_map;
	bool _material_dirty_flag = false;

	uint64_t _current_frame = 0; //Frame counter

	VulkanGraphicsDevice* vgd;		//Very dangerous and dubiously recommended
};
