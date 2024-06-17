#include "ImguiRenderer.h"
#include "utils.h"
#include <bit>
#include <inttypes.h>

ImguiRenderer::ImguiRenderer(
	VulkanGraphicsDevice* v,
	uint32_t sampler,
	ImVec2 window_size,
	Key<VkPipelineLayout> pipeline_layout_id,
	Key<VkRenderPass> renderpass
) {
    vgd = v;
    sampler_idx = sampler;

    //Initialize Dear ImGui
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
    	ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = window_size;

		//Build and upload font atlas
		uint8_t* tex_pixels = nullptr;
		int tex_w, tex_h;
		io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_w, &tex_h);
		RawImage im = {
			.width = (uint32_t)tex_w,
			.height = (uint32_t)tex_h,
			.data = tex_pixels
		};

		std::vector<RawImage> images = std::vector<RawImage>{
			im
		};
		std::vector<VkFormat> formats = std::vector<VkFormat>{
			VK_FORMAT_R8G8B8A8_UNORM
		};
		uint64_t batch_id = vgd->load_raw_images(images, formats);

		VkSemaphoreWaitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
		info.semaphoreCount = 1;
		info.pSemaphores = vgd->get_semaphore(vgd->image_upload_semaphore);
		info.pValues = &batch_id;
		if (vkWaitSemaphores(vgd->device, &info, U64_MAX) != VK_SUCCESS) {
			printf("Waiting for graphics timeline semaphore failed.\n");
			exit(-1);
		}
	
		uint32_t tex_index = 0;
		for (auto it = vgd->available_images.begin(); it != vgd->available_images.end(); ++it) {
			VulkanAvailableImage image = *it;
			if (batch_id == image.batch_id) {
				tex_index = it.slot_index();
				break;
			}
		}

		atlas_idx = tex_index;
		io.Fonts->SetTexID((ImTextureID)(uint64_t)tex_index);
	}

    //Allocate memory for ImGUI vertex data
    {
        VkDeviceSize buffer_size = 256 * 1024;		//256KB per vertex attribute

        VmaAllocationCreateInfo alloc_info = {};
        alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
        alloc_info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        alloc_info.priority = 1.0;

		VkBufferUsageFlags vertex_buffer_flags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        position_buffer = vgd->create_buffer(buffer_size, vertex_buffer_flags, alloc_info);
        uv_buffer = vgd->create_buffer(buffer_size, vertex_buffer_flags, alloc_info);
        color_buffer = vgd->create_buffer(buffer_size, vertex_buffer_flags, alloc_info);

		//Get buffer device address :)
		this->position_address = vgd->buffer_device_address(position_buffer);
		this->uv_address = vgd->buffer_device_address(uv_buffer);
		this->color_address = vgd->buffer_device_address(color_buffer);

        index_buffer = vgd->create_buffer(buffer_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, alloc_info);
    }

	//Create graphics pipeline
	{
		VulkanInputAssemblyState ia_states[] = {
			{}
		};

		VulkanTesselationState tess_states[] = {
			{}
		};

		VulkanViewportState vs_states[] = {
			{}
		};

		VulkanRasterizationState rast_states[] = {
			{
				.cullMode = VK_CULL_MODE_NONE
			},
		};

		VulkanMultisampleState ms_states[] = {
			{}
		};

		VulkanDepthStencilState ds_states[] = {
			{
				.depthTestEnable = VK_FALSE
			}
		};

		VulkanColorBlendAttachmentState blend_attachment_state = {};
		VulkanColorBlendState blend_states[] = {
			{
				.attachmentCount = 1,
				.pAttachments = &blend_attachment_state
			}
		};

		const char* shaders[] = { "shaders/imgui.vert.spv", "shaders/imgui.frag.spv" };

		graphics_pipeline_layout = pipeline_layout_id;
		vgd->create_graphics_pipelines(
			pipeline_layout_id,
			renderpass,
			shaders,
			ia_states,
			tess_states,
			vs_states,
			rast_states,
			ms_states,
			ds_states,
			blend_states,
			&graphics_pipeline,
			1
		);
	}

}

ImguiRenderer::~ImguiRenderer() {
	vgd->destroy_buffer(position_buffer);
	vgd->destroy_buffer(uv_buffer);
	vgd->destroy_buffer(color_buffer);
	vgd->destroy_buffer(index_buffer);
}

void ImguiRenderer::draw(VkCommandBuffer& frame_cb, uint64_t frame_counter) {
	//Upload ImGUI triangle data and record ImGUI draw commands
	
	uint32_t frame_slot = frame_counter % FRAMES_IN_FLIGHT;
	uint32_t last_frame_slot = frame_slot == 0 ? FRAMES_IN_FLIGHT - 1 : frame_slot - 1;
	ImguiFrame& current_frame = frames[frame_slot];
	ImguiFrame& last_frame = frames[last_frame_slot];

	ImGui::Render();
	ImDrawData* draw_data = ImGui::GetDrawData();

	//TODO: This local offset logic might only hold when FRAMES_IN_FLIGHT == 2
	uint32_t current_vertex_offset = 0;
	if (last_frame.vertex_start < (uint32_t)draw_data->TotalVtxCount) {
		current_vertex_offset = last_frame.vertex_start + last_frame.vertex_size;
	}
	current_frame.vertex_start = current_vertex_offset;
	current_frame.vertex_size = draw_data->TotalVtxCount;

	uint32_t current_index_offset = 0;
	if (last_frame.index_start < (uint32_t)draw_data->TotalIdxCount) {
		current_index_offset = last_frame.index_start + last_frame.index_size;
	}
	current_frame.index_start = current_index_offset;
	current_frame.index_size = draw_data->TotalIdxCount;

	VulkanBuffer* gpu_imgui_positions = vgd->get_buffer(position_buffer);
	VulkanBuffer* gpu_imgui_uvs = vgd->get_buffer(uv_buffer);
	VulkanBuffer* gpu_imgui_colors = vgd->get_buffer(color_buffer);
	VulkanBuffer* gpu_imgui_indices = vgd->get_buffer(index_buffer);
	
	uint8_t* gpu_pos_ptr = std::bit_cast<uint8_t*>(gpu_imgui_positions->alloc_info.pMappedData);
	uint8_t* gpu_uv_ptr = std::bit_cast<uint8_t*>(gpu_imgui_uvs->alloc_info.pMappedData);
	uint8_t* gpu_col_ptr = std::bit_cast<uint8_t*>(gpu_imgui_colors->alloc_info.pMappedData);
	uint8_t* gpu_idx_ptr = std::bit_cast<uint8_t*>(gpu_imgui_indices->alloc_info.pMappedData);

	//Record once-per-frame binding of index buffer and graphics pipeline
	vkCmdBindIndexBuffer(frame_cb, gpu_imgui_indices->buffer, current_index_offset * sizeof(ImDrawIdx), VK_INDEX_TYPE_UINT16);
	vkCmdBindPipeline(frame_cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vgd->get_graphics_pipeline(graphics_pipeline)->pipeline);

	ImguiPushConstants pcs = {
		.atlas_idx = atlas_idx,
		.sampler_idx = sampler_idx,
		.position_address = position_address,
		.uv_address = uv_address,
		.color_address = color_address
	};
	ImGuiIO& io = ImGui::GetIO();
	pcs.clip_matrix = hlslpp::float4x4(
		2.0f / (float)io.DisplaySize.x, 0.0f, 0.0f, -1.0f,
		0.0f, 2.0f / (float)io.DisplaySize.y, 0.0f, -1.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);
	
	//Create intermediate buffers for Imgui vertex attributes
	std::vector<uint8_t> imgui_positions;
	imgui_positions.resize(draw_data->TotalVtxCount * sizeof(float) * 2);
	std::vector<uint8_t> imgui_uvs;
	imgui_uvs.resize(draw_data->TotalVtxCount * sizeof(float) * 2);
	std::vector<uint8_t> imgui_colors;
	imgui_colors.resize(draw_data->TotalVtxCount * sizeof(uint32_t));

	uint32_t vtx_offset = 0;
	uint32_t idx_offset = 0;
	for (int32_t i = 0; i < draw_data->CmdListsCount; i++) {
		ImDrawList* draw_list = draw_data->CmdLists[i];

		//Copy vertex data into respective buffers
		for (int32_t j = 0; j < draw_list->VtxBuffer.Size; j++) {
			ImDrawVert* vert = draw_list->VtxBuffer.Data + j;
			uint32_t vector_offset = (j + vtx_offset) * 2 * sizeof(float);
			uint32_t int_offset = (j + vtx_offset) * sizeof(uint32_t);
			memcpy(imgui_positions.data() + vector_offset, &vert->pos, 2 * sizeof(float));
			memcpy(imgui_uvs.data() + vector_offset, &vert->uv, 2 * sizeof(float));
			memcpy(imgui_colors.data() + int_offset, &vert->col, sizeof(uint32_t));
		}

		//Copy index data
		size_t copy_size = draw_list->IdxBuffer.Size * sizeof(ImDrawIdx);
		size_t copy_offset = (idx_offset + current_index_offset) * sizeof(ImDrawIdx);
		memcpy(gpu_idx_ptr + copy_offset, draw_list->IdxBuffer.Data, copy_size);

		//Record draw commands
		for (int32_t j = 0; j < draw_list->CmdBuffer.Size; j++){
			ImDrawCmd& draw_command = draw_list->CmdBuffer[j];

			VkRect2D scissor = {
				.offset = {
					.x = (int32_t)draw_command.ClipRect.x,
					.y = (int32_t)draw_command.ClipRect.y
				},
				.extent = {
					.width = (uint32_t)draw_command.ClipRect.z - (uint32_t)draw_command.ClipRect.x,
					.height = (uint32_t)draw_command.ClipRect.w - (uint32_t)draw_command.ClipRect.y
				}
			};
			vkCmdSetScissor(frame_cb, 0, 1, &scissor);
			
			pcs.atlas_idx = (uint32_t)draw_command.TextureId;
			vkCmdPushConstants(
				frame_cb,
				*vgd->get_pipeline_layout(graphics_pipeline_layout),
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(ImguiPushConstants),
				&pcs
			);

			vkCmdDrawIndexed(frame_cb, draw_command.ElemCount, 1, draw_command.IdxOffset + idx_offset, draw_command.VtxOffset + current_vertex_offset + vtx_offset, 0);
		}
		vtx_offset += draw_list->VtxBuffer.Size;
		idx_offset += draw_list->IdxBuffer.Size;
	}

	//Finally copy the intermediate vertex buffers to the real ones on the GPU
	uint32_t vector_offset = current_vertex_offset * 2 * sizeof(float);
	uint32_t int_offset = current_vertex_offset * sizeof(uint32_t);
	memcpy(gpu_pos_ptr + vector_offset, imgui_positions.data(), imgui_positions.size());
	memcpy(gpu_uv_ptr + vector_offset, imgui_uvs.data(), imgui_uvs.size());
	memcpy(gpu_col_ptr + int_offset, imgui_colors.data(), imgui_colors.size());
	
}

uint32_t ImguiRenderer::get_atlas_idx() { return atlas_idx; }