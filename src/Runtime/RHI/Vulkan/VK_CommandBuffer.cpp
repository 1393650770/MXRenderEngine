#include "VK_CommandBuffer.h"
#include "VK_Device.h"
#include "VK_Fence.h"
#include "VK_Utils.h"
#include "VK_Define.h"
#include "VK_Buffer.h"
#include "VK_PipelineState.h"
#include "VK_RenderPass.h"
#include "VK_Texture.h"
#include "VK_FrameBuffer.h"
#include "Core/ConstDefine.h"
#include "Core/ConstGlobals.h"
#include "VK_Shader.h"
#define  GLFW_INCLUDE_VULKAN
#if !PLATFORM_ANDROID
#include "imgui_impl_vulkan.h"
#include "imgui_impl_glfw.h"
#endif
#if !PLATFORM_ANDROID
#include "GLFW/glfw3.h"
#endif

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)


VK_CommandBufferPool::VK_CommandBufferPool(VK_Device* in_device, VK_CommandBufferManager& in_cmd_buffer_manager):device(in_device),cmd_buffer_manager(in_cmd_buffer_manager)
{

}

VK_CommandBufferPool::~VK_CommandBufferPool()
{
	for (UInt32 index = 0; index < cmd_buffers.size(); ++index)
	{
		delete cmd_buffers[index];
	}
	for (UInt32 index = 0; index < free_cmd_buffers.size(); ++index)
	{
		delete free_cmd_buffers[index];
	}
	vkDestroyCommandPool(device->GetDevice(), command_pool, VULKAN_CPU_ALLOCATOR);
	command_pool=VK_NULL_HANDLE;
}

VK_CommandBuffer* VK_CommandBufferPool::GetOrCreateCommandBuffer(Bool is_upload_only)
{
	for (Int index = free_cmd_buffers.size()-1; index>=0;--index)
	{
		VK_CommandBuffer* cmd_buffer = free_cmd_buffers[index];
		if (cmd_buffer->is_upload_only == is_upload_only)
		{
			free_cmd_buffers.erase(free_cmd_buffers.begin() + index);
			cmd_buffer->Allocate();
			cmd_buffer->GetFence()->ResetFence();  // reset fence from previous submit
			cmd_buffers.push_back(cmd_buffer);
			return cmd_buffer;
		}
	}
	VK_CommandBuffer* cmd_buffer = new VK_CommandBuffer(device, this, is_upload_only);
	cmd_buffers.push_back(cmd_buffer);
	return cmd_buffer;
}

void VK_CommandBufferPool::Init(UInt32 queue_family_index)
{
	VkCommandPoolCreateInfo command_pool_create_info;
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = queue_family_index;
	command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	command_pool_create_info.pNext = nullptr;
	CHECK_WITH_LOG(vkCreateCommandPool(device->GetDevice(), &command_pool_create_info, VULKAN_CPU_ALLOCATOR, &command_pool) != VK_SUCCESS, "RHI Error: Create CommandPool Error");
}

void VK_CommandBufferPool::FreeUnusedCommandBuffers(VK_Queue* queue)
{
	for (Int index = cmd_buffers.size() - 1; index >= 0; --index)
	{
		VK_CommandBuffer* cmd_buffer = cmd_buffers[index];
		if ( cmd_buffer->GetFence()->CheckSignaled())
		{
			cmd_buffer->Free();
			free_cmd_buffers.push_back(cmd_buffer);
			cmd_buffers.erase(cmd_buffers.begin() + index);
			return;
		}
	}
}

void VK_CommandBufferPool::FreeUnusedCommandBuffer(VK_CommandBuffer* target)
{
	for (Int index = cmd_buffers.size() - 1; index >= 0; --index)
	{
		VK_CommandBuffer* cmd_buffer = cmd_buffers[index];
		if (target == cmd_buffer && cmd_buffer->GetFence()->CheckSignaled())
		{
			cmd_buffer->Free();
			free_cmd_buffers.push_back(cmd_buffer);
			cmd_buffers.erase(cmd_buffers.begin() + index);
			return;
		}
	}
}

VkCommandPool VK_CommandBufferPool::GetPool() CONST
{
	return command_pool;
}


VK_CommandBuffer::~VK_CommandBuffer()
{
	if (false)
	{
		// Wait 33ms
		UInt64 wait_seconds = 33 * 1000 * 1000LL;
		device->GetFenceManager()->WaitForFence(fence, wait_seconds);
	}
	if (fence)
	{
		device->GetFenceManager()->FreeFence(fence);
		fence=nullptr;
	}
	if(command_buffer!=VK_NULL_HANDLE)
	{ 
		Free();
		command_buffer=VK_NULL_HANDLE;
	}
}

VkCommandBuffer VK_CommandBuffer::GetCommandBuffer() CONST
{
	return command_buffer;
}

VK_Fence* VK_CommandBuffer::GetFence() CONST
{
	return fence;
}

void VK_CommandBuffer::Allocate()
{
	VkCommandBufferAllocateInfo allocate_info{};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.commandBufferCount =1;
	allocate_info.level= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandPool=owner_pool->GetPool();
	CHECK_WITH_LOG(vkAllocateCommandBuffers(device->GetDevice(), &allocate_info, &command_buffer) != VK_SUCCESS,
		"RHI Error: failed to allocate command buffers!");
}

void VK_CommandBuffer::Free()
{
	CHECK_WITH_LOG(command_buffer==VK_NULL_HANDLE,"RHI Error: command buffer is nullptr when freeing commandbuffer!");
	vkFreeCommandBuffers(device->GetDevice(), owner_pool->GetPool(), 1, &command_buffer);
	command_buffer=VK_NULL_HANDLE;
	
}

void VK_CommandBuffer::ClearTexture(Texture* texture, Vector<float> clear_value /*= Vector<float>(4, 0.0f)*/)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdClearTexture>(texture, std::move(clear_value))); return; }
	static constexpr CONST UInt32 invalid_attachment_index = ~UInt32{ 0 };

	bool clear_depth_attachment = false,clear_depth_image=false;
	UInt32 attachment_index = invalid_attachment_index;
	for (UInt32 rt = 0; rt < state_cache.render_targets.size(); ++rt)
	{
		if (state_cache.render_targets[rt] == texture)
		{
			attachment_index = rt;
			break;
		}
	}
	if (attachment_index == invalid_attachment_index)
	{
		if (state_cache.depth_stencil == texture)
		{
			clear_depth_attachment = true;
		}
		else
		{
			if ((UInt32)(texture->GetTextureDesc().usage & ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_DEPTH_ATTACHMENT) != 0 ||
				(UInt32)(texture->GetTextureDesc().usage & ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_DEPTH_ATTACHMENT_READ_ONLY) != 0 ||
				(UInt32)(texture->GetTextureDesc().usage & ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_DEPTH_ATTACHMENT_WRITE_ONLY) != 0
				)
			{
				clear_depth_image = true;
			}
		}
	}
	if (attachment_index != invalid_attachment_index)
	{
		VkClearAttachment clear_attachment{};
		clear_attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		clear_attachment.colorAttachment = attachment_index;
		clear_attachment.clearValue = { clear_value[0], clear_value[1], clear_value[2], clear_value[3] };
		VkClearRect clear_rect{};
		clear_rect.baseArrayLayer = 0;
		clear_rect.layerCount = texture->GetTextureDesc().layer_count;
		clear_rect.rect.offset = { 0, 0 };
		clear_rect.rect.extent = { texture->GetTextureDesc().width, texture->GetTextureDesc().height };
		ClearAttachment(clear_attachment, clear_rect);
	}
	else if (clear_depth_attachment)
	{
		VkClearAttachment clear_attachment{};
		clear_attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		clear_attachment.clearValue.depthStencil.depth = clear_value[0];
		clear_attachment.clearValue.depthStencil.stencil = static_cast<UInt32>(clear_value[1]);
		VkClearRect clear_rect{};
		clear_rect.baseArrayLayer = 0;
		clear_rect.layerCount = texture->GetTextureDesc().layer_count;
		clear_rect.rect.offset = { 0, 0 };
		clear_rect.rect.extent = { texture->GetTextureDesc().width, texture->GetTextureDesc().height };
		ClearAttachment(clear_attachment, clear_rect);
	}
	else if (clear_depth_image)
	{
		TransitionTextureState(texture, ENUM_RESOURCE_STATE::CopyDest);
		VkImageSubresourceRange subresource_range{};
		subresource_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		subresource_range.baseArrayLayer = 0;
		subresource_range.layerCount = texture->GetTextureDesc().layer_count;
		subresource_range.baseMipLevel = 0;
		subresource_range.levelCount = texture->GetTextureDesc().mip_level;
		VkClearDepthStencilValue clear_value_depth_stencil{};
		clear_value_depth_stencil.depth = clear_value[0];
		clear_value_depth_stencil.stencil = static_cast<UInt32>(clear_value[1]);
		ClearDepthStencilImage(((VK_Texture*)texture)->GetImage(), clear_value_depth_stencil, subresource_range);
	}
	else
	{
		TransitionTextureState(texture, ENUM_RESOURCE_STATE::CopyDest);
		VkImageSubresourceRange subresource_range{};
		subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresource_range.baseArrayLayer = 0;
		subresource_range.layerCount = texture->GetTextureDesc().layer_count;
		subresource_range.baseMipLevel = 0;
		subresource_range.levelCount = texture->GetTextureDesc().mip_level;
		VkClearColorValue clear_color_value{};
		for (UInt32 i = 0; i < 4; ++i)
		{
			clear_color_value.float32[i] = clear_value[i];
		}
		ClearColorImage(((VK_Texture*)texture)->GetImage(), clear_color_value, subresource_range);

	}
}


void VK_CommandBuffer::BeginDynamicRendering(CONST Vector<Texture*>& render_targets, Texture* depth_stencil, UInt32 width, UInt32 height, UInt32 clear_value_count, CONST VkClearValue* clear_values)
{
	if (!bypass) return;
	if (command_state == EState::IsInsideRenderPass)
		EndDynamicRendering();
	FlushBarriers();
	VkRenderingInfo ri{};
	ri.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	ri.renderArea = { {0, 0}, {width, height} };
	ri.layerCount = 1;
	Vector<VkRenderingAttachmentInfo> colors;
	for (UInt32 i = 0; i < render_targets.size(); ++i)
	{
		VK_Texture* vk_tex = STATIC_CAST(render_targets[i], VK_Texture);
		VkRenderingAttachmentInfo att{};
		att.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		att.imageView = vk_tex->GetImageView();
		att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		att.loadOp = (clear_value_count > i) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		if (clear_value_count > i && clear_values) att.clearValue = clear_values[i];
		colors.push_back(att);
	}
	ri.colorAttachmentCount = (UInt32)colors.size();
	ri.pColorAttachments = colors.data();
	VkRenderingAttachmentInfo depth{};
	if (depth_stencil)
	{
		VK_Texture* vk_ds = STATIC_CAST(depth_stencil, VK_Texture);
		depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depth.imageView = vk_ds->GetImageView();
		depth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depth.clearValue.depthStencil = {1.0f, 0};
		ri.pDepthAttachment = &depth;
	}
	vkCmdBeginRendering(command_buffer, &ri);
	command_state = EState::IsInsideRenderPass;
	state_cache.render_pass = VK_NULL_HANDLE;
	state_cache.framebuffer_width = width;
	state_cache.framebuffer_height = height;
}
void VK_CommandBuffer::TransitionTextureState(Texture* texture, CONST ENUM_RESOURCE_STATE& required_state)
{
	// --   Recording: only push command, never modify CB live state (image_barriers etc.)
	//       All live state modification happens during replay on RHI thread.
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdTransitionTexture>(texture, required_state)); return; }
	if (texture->GetTextureDesc().resource_state != required_state)
	{
		ENUM_RESOURCE_STATE old_state = texture->GetTextureDesc().resource_state;
		VkImageSubresourceRange subres_range;
		subres_range.aspectMask = 0;
		subres_range.baseArrayLayer = 0;
		subres_range.layerCount = VK_REMAINING_ARRAY_LAYERS;
		subres_range.baseMipLevel = 0;
		subres_range.levelCount = VK_REMAINING_MIP_LEVELS;
		if (subres_range.aspectMask == 0)
		{
			CONST auto& tex_desc = texture->GetTextureDesc();
			CONST auto& fmt_attribs = texture->GetTextureFormatAttribs(tex_desc.format);
			if (fmt_attribs.component_format == ENUM_TEXTURE_COMPONENT_FORMAT::Depth)
				subres_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			else if (fmt_attribs.component_format == ENUM_TEXTURE_COMPONENT_FORMAT::DepthStencil)
			{
				// If image has a depth / stencil format with both depth and stencil components, then the
				// aspectMask member of subresourceRange must include both VK_IMAGE_ASPECT_DEPTH_BIT and
				// VK_IMAGE_ASPECT_STENCIL_BIT 
				subres_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			}
			else
				subres_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
		// Always add barrier after writes.
		CONST Bool after_write = VK_Utils::Check_ResourceState_Has_WriteAccess(old_state);

		CONST auto  old_layout = VK_Utils::Translate_ReourceState_To_VulkanImageLayout(old_state,command_state==EState::IsInsideRenderPass);
		CONST auto  new_layout = VK_Utils::Translate_ReourceState_To_VulkanImageLayout(required_state, command_state == EState::IsInsideRenderPass);
		CONST auto  old_stages  =  VK_Utils::Translate_ReourceState_To_VulkanPipelineStage(old_state);
		CONST auto  new_stages  =  VK_Utils::Translate_ReourceState_To_VulkanPipelineStage(required_state);

		if (((old_state & required_state) != required_state) || old_layout != new_layout || after_write)
		{
			VK_Texture* vk_texture = STATIC_CAST( texture, VK_Texture);
			TrainsitionImageLayout(vk_texture->GetImage(), old_layout, new_layout, subres_range, old_stages, new_stages);
			texture->SetResourceState(required_state);
		}
	}

}

#if !PLATFORM_ANDROID
void VK_CommandBuffer::BeginUI()
{
	// CPU阶段：始终在主线程执行（NewFrame 必须与 GLFW 输入同步）
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// record 模式下录制命令，bypass 模式下无需额外操作（GPU 工作由 EndUI 完成）
	if (!bypass)
	{
		recorded_commands.push_back(std::make_unique<RHICmdBeginUI>());
	}
}

void VK_CommandBuffer::EndUI()
{
	// CPU阶段：Render=EndFrame
	ImGui::Render();

	//  三线程模式：平台窗口更新推迟到 Logic 线程（EndUI_Platform）
	// GLFW CreateWindow/DestroyWindow 必须在主线程
	if (g_thread_mode != EThreadingMode::ThreeThread)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
	}

	// GPU阶段：record 模式录制命令待 RHI 线程重放，bypass 模式直接提交 Vulkan 命令
	if (!bypass)
	{
		recorded_commands.push_back(std::make_unique<RHICmdEndUI>());
		return;
	}
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
}

// 三线程模式：拆分 UI 阶段实现

void VK_CommandBuffer::BeginUI_Logic()
{
	// Logic 线程（GLFW 线程安全）：捕获输入 + ImGui NewFrame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	if (!bypass)
	{
		recorded_commands.push_back(std::make_unique<RHICmdBeginUI>());
	}
}

void VK_CommandBuffer::EndUI_Render()
{
	// Render 线程：生成 draw data + 录制 GPU 命令
	ImGui::Render();

	if (!bypass)
	{
		recorded_commands.push_back(std::make_unique<RHICmdEndUI>());
		return;
	}
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
}

void VK_CommandBuffer::EndUI_Platform()
{
	// Logic 线程（GLFW 线程安全）：平台窗口更新
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow* backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup_current_context);
	}
}
#else  // PLATFORM_ANDROID — stubs for ImGui methods
void VK_CommandBuffer::BeginUI() {}
void VK_CommandBuffer::EndUI() {}
void VK_CommandBuffer::BeginUI_Logic() {}
void VK_CommandBuffer::EndUI_Render() {}
void VK_CommandBuffer::EndUI_Platform() {}
#endif // !PLATFORM_ANDROID

// =========================================================================
// UI Scissor support (used by VK_RmlRenderer)
// =========================================================================
void VK_CommandBuffer::SetScissorEnable(bool enable)
{
	state_cache.scissor_enabled = enable;
	if (bypass)
	{
		if (enable)
			vkCmdSetScissor(command_buffer, 0, 1, &state_cache.scissor);
		else
		{
			VkRect2D full = { {0, 0}, {state_cache.framebuffer_width, state_cache.framebuffer_height} };
			vkCmdSetScissor(command_buffer, 0, 1, &full);
		}
	}
	else
	{
		recorded_commands.push_back(std::make_unique<RHICmdSetScissorEnable>(enable));
	}
}

void VK_CommandBuffer::SetScissor(Int x, Int y, UInt32 w, UInt32 h)
{
	state_cache.scissor.offset.x = x;
	state_cache.scissor.offset.y = y;
	state_cache.scissor.extent.width = w;
	state_cache.scissor.extent.height = h;
	if (state_cache.scissor_enabled)
	{
		if (bypass)
			vkCmdSetScissor(command_buffer, 0, 1, &state_cache.scissor);
		else
			recorded_commands.push_back(std::make_unique<RHICmdSetScissor>(x, y, w, h));
	}
}

//

void VK_CommandBuffer::TransitionRenderTargets(CONST Vector<Texture*>& render_targets, Texture* depth_stencil)
{
	if (render_targets.empty() && depth_stencil == nullptr)
	{
		return;
	}
	if (!render_targets.empty())
	{
		for (auto& rtv : render_targets)
		{
			TransitionTextureState(rtv, ENUM_RESOURCE_STATE::RenderTarget);
		}
	}
	if (depth_stencil != nullptr)
	{
		TransitionTextureState(depth_stencil, ENUM_RESOURCE_STATE::DepthWrite);
	}
}

VK_CommandBuffer::VK_CommandBuffer(VK_Device* in_device, VK_CommandBufferPool* in_command_buffer_pool, Bool in_is_upload_only):
		device(in_device),
		owner_pool(in_command_buffer_pool),
		is_upload_only(in_is_upload_only)
{
	Allocate();
	fence=device->GetFenceManager()->GetOrCreateFence();
	command_state = EState::ReadyForBegin;
}

UInt64 VK_CommandBuffer::GetFenceSignaledCounter() CONST
{
	return fence_signaled_counter;
}

Bool VK_CommandBuffer::WaitForFence(float time_in_seconds_to_wait)
{
	UInt64 timeout_ns = (time_in_seconds_to_wait <= 0.0f)
		? 0ULL
		: (UInt64)(time_in_seconds_to_wait * 1'000'000'000.0);
	return device->GetFenceManager()->WaitForFence(fence, timeout_ns);
}

void VK_CommandBuffer::TrainsitionImageLayout(VkImage image, VkImageLayout old_layout, VkImageLayout new_layout,CONST VkImageSubresourceRange& subresource_range, VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask)
{
	//if (state_cache.render_pass != VK_NULL_HANDLE)
	//{
	//	EndRenderPass();
	//}

	if (old_layout == new_layout)
	{
		pipeline_barrier.memory_src_stages |= src_stage_mask;
		pipeline_barrier.memory_dst_stages |= dst_stage_mask;

		pipeline_barrier.memory_src_access |= VK_Utils::AccessMaskFromImageLayout(old_layout,false);
		pipeline_barrier.memory_dst_access |= VK_Utils::AccessMaskFromImageLayout(new_layout,false);
	}

	// Check overlapping subresources
	for (size_t i = 0; i < image_barriers.size(); ++i)
	{
		CONST auto& ImgBarrier = image_barriers[i];
		if (ImgBarrier.image != image)
			continue;

		CONST auto& other_range = ImgBarrier.subresourceRange;

		CONST auto start_layer0 = subresource_range.baseArrayLayer;
		CONST auto end_layer0 = subresource_range.layerCount != VK_REMAINING_ARRAY_LAYERS ? (subresource_range.baseArrayLayer + subresource_range.layerCount) : ~0u;
		CONST auto start_layer1 = other_range.baseArrayLayer;
		CONST auto end_layer1 = other_range.layerCount != VK_REMAINING_ARRAY_LAYERS ? (other_range.baseArrayLayer + other_range.layerCount) : ~0u;

		CONST auto start_mip0 = subresource_range.baseMipLevel;
		CONST auto end_mip0 = subresource_range.levelCount != VK_REMAINING_MIP_LEVELS ? (subresource_range.baseMipLevel + subresource_range.levelCount) : ~0u;
		CONST auto start_mip1 = other_range.baseMipLevel;
		CONST auto end_mip1 = other_range.levelCount != VK_REMAINING_MIP_LEVELS ? (other_range.baseMipLevel + other_range.levelCount) : ~0u;

		CONST auto is_slices_overlap = CheckLineSectionOverlap<true>(start_layer0, end_layer0, start_layer1, end_layer1);
		CONST auto is_mips_overlap = CheckLineSectionOverlap<true>(start_mip0, end_mip0, start_mip1, end_mip1);

		// If the range overlaps with any of the existing barriers, we need to
		// flush them.
		if (is_slices_overlap && is_mips_overlap)
		{
			FlushBarriers();
			break;
		}
	}

	pipeline_barrier.image_src_stages |= src_stage_mask;
	pipeline_barrier.image_dst_stages |= dst_stage_mask;

	VkImageMemoryBarrier ImgBarrier{};
	ImgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	ImgBarrier.pNext = nullptr;
	ImgBarrier.oldLayout = old_layout;
	ImgBarrier.newLayout = new_layout;
	ImgBarrier.image = image;
	ImgBarrier.subresourceRange = subresource_range;
	ImgBarrier.srcAccessMask = VK_Utils::AccessMaskFromImageLayout(old_layout, false) & pipeline_barrier.supported_access_mask;
	ImgBarrier.dstAccessMask = VK_Utils::AccessMaskFromImageLayout(new_layout, true) & pipeline_barrier.supported_access_mask;
	ImgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // source queue family for a queue family ownership transfer.
	ImgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // destination queue family for a queue family ownership transfer.
	image_barriers.emplace_back(ImgBarrier);
}

void VK_CommandBuffer::FlushBarriers()
{
	// --   Recording: only push command, never touch CB live state (image_barriers etc.)
	//       Barrier accumulation and flushing all happen during replay on RHI thread.
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdFlushBarriers>()); return; }

	// --   Bypass mode: execute barriers directly (on RHI thread during replay)
	if (state_cache.render_pass != VK_NULL_HANDLE)
	{
		EndRenderPass();
	}
	if(pipeline_barrier.memory_src_stages==0&&pipeline_barrier.memory_dst_stages==0&&image_barriers.empty())
	{
		return;
	}
	VkMemoryBarrier vk_mem_barrier{};
	vk_mem_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	vk_mem_barrier.pNext = nullptr;
	vk_mem_barrier.srcAccessMask = pipeline_barrier.memory_src_access & pipeline_barrier.supported_access_mask;
	vk_mem_barrier.dstAccessMask = pipeline_barrier.memory_dst_access & pipeline_barrier.supported_access_mask;

	CONST Bool HasMemoryBarrier =
		pipeline_barrier.memory_src_stages != 0 && pipeline_barrier.memory_dst_stages != 0 &&
		pipeline_barrier.memory_src_access != 0 && pipeline_barrier.memory_dst_access != 0;

	CONST VkPipelineStageFlags SrcStages = (pipeline_barrier.image_src_stages | pipeline_barrier.memory_src_stages) & pipeline_barrier.supported_stages_mask;
	CONST VkPipelineStageFlags DstStages = (pipeline_barrier.image_dst_stages | pipeline_barrier.memory_dst_stages) & pipeline_barrier.supported_stages_mask;

	vkCmdPipelineBarrier(command_buffer,
		SrcStages,
		DstStages,
		0,
		HasMemoryBarrier ? 1 : 0,
		HasMemoryBarrier ? &vk_mem_barrier : nullptr,
		0,
		nullptr,
		static_cast<UInt32>(image_barriers.size()),
		image_barriers.empty() ? nullptr : image_barriers.data());

	image_barriers.clear();
	pipeline_barrier.image_src_stages = 0;
	pipeline_barrier.image_dst_stages = 0;
	pipeline_barrier.memory_src_stages = 0;
	pipeline_barrier.memory_dst_stages = 0;
	pipeline_barrier.memory_src_access = 0;
	pipeline_barrier.memory_dst_access = 0;
}

void VK_CommandBuffer::MemoryBarrier(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
{
	if (state_cache.render_pass != VK_NULL_HANDLE)
	{
		EndRenderPass();
	}


	pipeline_barrier.memory_src_stages |= srcStageMask;
	pipeline_barrier.memory_dst_stages |= dstStageMask;

	pipeline_barrier.memory_src_access |= srcAccessMask;
	pipeline_barrier.memory_dst_access |= dstAccessMask;
}

// --   RHI barrier API implementations

void VK_CommandBuffer::ResourceBarrier(ENUM_RESOURCE_STATE src_state, ENUM_RESOURCE_STATE dst_state)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdResourceBarrier>(src_state, dst_state)); return; }
    if (state_cache.render_pass != VK_NULL_HANDLE)
    {
        EndRenderPass();
    }

    VkPipelineStageFlags srcStage = VK_Utils::Translate_ReourceState_To_VulkanPipelineStage(src_state);
    VkPipelineStageFlags dstStage = VK_Utils::Translate_ReourceState_To_VulkanPipelineStage(dst_state);

    // Full per-bit access translation (fixes IndirectArgument/CopyDest/etc.
    // producing SHADER_READ instead of the correct access bit).
    VkAccessFlags srcAccess = VK_Utils::Translate_ReourceState_To_VulkanAccess(src_state);
    VkAccessFlags dstAccess = VK_Utils::Translate_ReourceState_To_VulkanAccess(dst_state);

    pipeline_barrier.memory_src_stages |= srcStage;
    pipeline_barrier.memory_dst_stages |= dstStage;
    pipeline_barrier.memory_src_access |= srcAccess;
    pipeline_barrier.memory_dst_access |= dstAccess;
}

void VK_CommandBuffer::MemoryBarrier(ENUM_SHADER_STAGE src_stage, ENUM_SHADER_STAGE dst_stage, ENUM_RESOURCE_STATE src_access, ENUM_RESOURCE_STATE dst_access)
{
    if (state_cache.render_pass != VK_NULL_HANDLE)
    {
        EndRenderPass();
    }

    VkShaderStageFlagBits vkSrcStage = VK_Utils::Translate_ShaderTypeEnum_To_Vulkan(src_stage);
    VkShaderStageFlagBits vkDstStage = VK_Utils::Translate_ShaderTypeEnum_To_Vulkan(dst_stage);

    VkAccessFlags vkSrcAccess = VK_Utils::Check_ResourceState_Has_WriteAccess(src_access)
        ? VK_ACCESS_SHADER_WRITE_BIT
        : VK_ACCESS_SHADER_READ_BIT;
    VkAccessFlags vkDstAccess = VK_Utils::Check_ResourceState_Has_WriteAccess(dst_access)
        ? VK_ACCESS_SHADER_WRITE_BIT
        : VK_ACCESS_SHADER_READ_BIT;

    pipeline_barrier.memory_src_stages |= vkSrcStage;
    pipeline_barrier.memory_dst_stages |= vkDstStage;
    pipeline_barrier.memory_src_access |= vkSrcAccess;
    pipeline_barrier.memory_dst_access |= vkDstAccess;
}
// --  

void VK_CommandBuffer::CopyBufferToImage(VkBuffer buffer, VkImage image, VkImageLayout imageLayout,UInt32 region_count, CONST VkBufferImageCopy* region)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdCopyBufferToImage>((UInt64)(uintptr_t)buffer, (UInt64)(uintptr_t)image, (Int)imageLayout, region_count, region, region_count * sizeof(VkBufferImageCopy))); return; }
	if (state_cache.render_pass != VK_NULL_HANDLE)
	{
		EndRenderPass();
	}
	FlushBarriers();
	vkCmdCopyBufferToImage(command_buffer, buffer, image, imageLayout, region_count, region);

}


void VK_CommandBuffer::SetGraphicsPipeline(RenderPipelineState* pipeline_state)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdSetGraphicsPipeline>(pipeline_state)); return; }
	CHECK(pipeline_state == nullptr);
	VK_PipelineState * vk_pipeline_state = STATIC_CAST(pipeline_state, VK_PipelineState);
	VkPipeline graphics_pipeline = vk_pipeline_state->GetPipeline();
	VkPipelineLayout pipeline_layout = vk_pipeline_state->GetPipelineLayout();
	if (state_cache.graphics_pipeline != graphics_pipeline)
	{
		state_cache.graphics_pipeline = graphics_pipeline;
		state_cache.pipeline_layout = pipeline_layout;
	}
}

void VK_CommandBuffer::SetComputePipeline(RenderPipelineState* pipeline_state)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdSetComputePipeline>(pipeline_state)); return; }
	CHECK(pipeline_state == nullptr);
	VK_PipelineState* vk_pipeline_state = STATIC_CAST(pipeline_state, VK_PipelineState);
	VkPipeline compute_pipeline = vk_pipeline_state->GetPipeline();
	VkPipelineLayout pipeline_layout = vk_pipeline_state->GetPipelineLayout();
	if (state_cache.compute_pipeline != compute_pipeline)
	{
		state_cache.compute_pipeline = compute_pipeline;
		state_cache.pipeline_layout = pipeline_layout;
		// --   removed immediate vkCmdBindPipeline -- deferred to Dispatch(),
		// matching the SetGraphicsPipeline pattern where binding happens in Draw().
	}
}

void VK_CommandBuffer::Dispatch(UInt32 groupX, UInt32 groupY, UInt32 groupZ)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdDispatch>(groupX, groupY, groupZ)); return; }
	// Compute dispatches must happen outside an active render pass
	if (state_cache.render_pass != VK_NULL_HANDLE)
	{
		EndRenderPass();
	}
	FlushBarriers();
	// Flush any deferred descriptor writes before binding
	if (state_cache.srb)
		state_cache.srb->FlushDescriptorWrites();
	// --   bind pipeline here (deferred from SetComputePipeline), matching Draw() pattern
	if (state_cache.compute_pipeline != VK_NULL_HANDLE)
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, state_cache.compute_pipeline);
	if (state_cache.descriptor_sets != nullptr)
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, state_cache.pipeline_layout, state_cache.first_set, state_cache.descriptor_sets_count, state_cache.descriptor_sets, 0, nullptr);
	vkCmdDispatch(command_buffer, groupX, groupY, groupZ);

}

void VK_CommandBuffer::SetPushConstants(UInt32 offset, UInt32 size, const void* data)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdSetPushConstants>(offset, size, data)); return; }
        if (state_cache.pipeline_layout == VK_NULL_HANDLE)
                return;
        // --   Auto-detect stage: compute vs graphics pipeline
        VkShaderStageFlagBits stage = (state_cache.compute_pipeline != VK_NULL_HANDLE)
                ? VK_SHADER_STAGE_COMPUTE_BIT
                : VkShaderStageFlagBits(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
        vkCmdPushConstants(command_buffer, state_cache.pipeline_layout,
                stage, offset, size, data);
}

// --   Stage-aware push constants overload
void VK_CommandBuffer::SetPushConstants(UInt32 offset, UInt32 size, const void* data, ENUM_SHADER_STAGE stage)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdSetPushConstants>(offset, size, data, stage)); return; }
	if (state_cache.pipeline_layout == VK_NULL_HANDLE)
		return;
	VkShaderStageFlagBits vk_stage = VK_Utils::Translate_ShaderTypeEnum_To_Vulkan(stage);
	vkCmdPushConstants(command_buffer, state_cache.pipeline_layout,
		vk_stage, offset, size, data);
}

// --   Combined compute dispatch: pipeline + SRB + dispatch in one call
void VK_CommandBuffer::ComputeDispatch(RenderPipelineState* pipeline, ShaderResourceBinding* srb, UInt32 groupX, UInt32 groupY, UInt32 groupZ)
{
        SetComputePipeline(pipeline);
        SetShaderResourceBinding(srb);
        Dispatch(groupX, groupY, groupZ);
}

void VK_CommandBuffer::SetRenderTarget(CONST Vector<Texture*>& render_targets, Texture* depth_stencil, CONST Vector<ClearValue>& clear_values, Bool has_dsv_clear_value)
{
	if (!bypass) { FlushBarriers(); recorded_commands.push_back(std::make_unique<RHICmdSetRenderTarget>(render_targets, depth_stencil, clear_values, has_dsv_clear_value)); return; }
	CHECK_WITH_LOG(command_state < EState::IsInsideBegin, "RHI Error: SetRenderTarget must be called between Begin and End");
	if (render_targets.empty()) return;
	// Dynamic rendering path (VK_KHR_dynamic_rendering)
	if (device->GetOptionalExtensions().HasKHRDynamicRendering)
	{
		TransitionRenderTargets(render_targets, depth_stencil);
		Vector<VkClearValue> vk_clear_values;
		for (Int i = 0; i < clear_values.size(); ++i)
			vk_clear_values.push_back({ clear_values[i].color[0], clear_values[i].color[1], clear_values[i].color[2], clear_values[i].color[3] });
		BeginDynamicRendering(render_targets, depth_stencil, render_targets[0]->GetTextureDesc().width, render_targets[0]->GetTextureDesc().height, (UInt32)vk_clear_values.size(), vk_clear_values.data());
		state_cache.render_targets = render_targets;
		state_cache.depth_stencil = depth_stencil;
		return;
	}
	// Legacy VkRenderPass path
	Vector<ENUM_TEXTURE_FORMAT> rtv_formats;
	for (auto& rtv : render_targets)
	{
		rtv_formats.push_back(rtv->GetTextureDesc().format);
	}
	ENUM_TEXTURE_FORMAT depth_stencil_format = ENUM_TEXTURE_FORMAT::None;
	if(depth_stencil!=nullptr)
		depth_stencil_format = depth_stencil->GetTextureDesc().format;
	RenderPassCacheKey renderpass_key(render_targets.size(), rtv_formats.data(), depth_stencil_format, render_targets[0]->GetTextureDesc().samples, false, false);
	VK_RenderPass* vk_renderpass = device->GetRenderPassManager()->GetRenderPass(renderpass_key);

	VkRenderPass render_pass = vk_renderpass->GetRenderPass();

	FramebufferCacheKey framebuffer_key;
	framebuffer_key.render_targets = render_targets;
	framebuffer_key.depth_stencil = depth_stencil;
	framebuffer_key.render_pass = render_pass;
	VK_FrameBuffer* vk_framebuffer = device->GetFrameBufferManager()->GetFramebuffer(framebuffer_key, render_targets[0]->GetTextureDesc().width, render_targets[0]->GetTextureDesc().height, 1);

	VkFramebuffer framebuffer = vk_framebuffer->GetFramebuffer();
	
	TransitionRenderTargets(render_targets,depth_stencil);

	Vector<VkClearValue> vk_clear_values;
	Int size = clear_values.size();

	for (Int i = 0; size > 0 &&i < size;++i)
	{
		vk_clear_values.push_back( { clear_values[i].color[0], clear_values[i].color[1], clear_values[i].color[2], clear_values[i].color[3] });
	}
	//if (has_dsv_clear_value)
	//{
	//	size = clear_values.size() - 1;
	//	if (size > 0)
	//		vk_clear_values.push_back({ clear_values[size].ds_value[0], clear_values[size].ds_value[1] });
	//}
	if (state_cache.render_pass != render_pass || state_cache.framebuffer != framebuffer)
	{
		state_cache.render_targets = render_targets;
		state_cache.depth_stencil = depth_stencil;
	}
	BeginRenderPass(render_pass, framebuffer, render_targets[0]->GetTextureDesc().width, render_targets[0]->GetTextureDesc().height, vk_clear_values.size(),vk_clear_values.data());

}

void VK_CommandBuffer::SetShaderResourceBinding(ShaderResourceBinding* srb)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdSetSRB>(srb)); return; }
	CHECK(srb == nullptr);
	VK_ShaderResourceBinding* vk_srb = STATIC_CAST(srb, VK_ShaderResourceBinding);
	state_cache.srb = vk_srb;
	CONST VkDescriptorSet* descriptor_sets = vk_srb->GetDescriptorSets();
	// Find the first and last non-null descriptor set in the array.
	// This correctly handles non-contiguous sets (e.g., Set 0 and Set 3 used, Sets 1-2 null).
	Int first_non_null = -1;
	Int last_non_null = -1;
	for (UInt32 i = 0; i < MYRENDER_MAX_BINDING_SET_NUM; ++i)
	{
		if (descriptor_sets[i] != VK_NULL_HANDLE)
		{
			if (first_non_null < 0) first_non_null = (Int)i;
			last_non_null = (Int)i;
		}
	}
	if (first_non_null >= 0)
	{
		state_cache.descriptor_sets = &(descriptor_sets[first_non_null]);
		state_cache.descriptor_sets_count = (UInt8)(last_non_null - first_non_null + 1);
		state_cache.first_set = (UInt32)first_non_null;
	}
	else
	{
		state_cache.descriptor_sets = nullptr;
		state_cache.descriptor_sets_count = 0;
		state_cache.first_set = 0;
	}
}

void VK_CommandBuffer::Draw(CONST DrawAttribute& draw_attr)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdDraw>(draw_attr)); return; }
	//vkCmdPushConstants(GetCommandBuffer(), state_cache.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &z);
	VkViewport viewport{};
	viewport.x = 0.0f; viewport.y = 0.0f;
	viewport.width = state_cache.framebuffer_width;
	viewport.height = state_cache.framebuffer_height;
	viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { static_cast<UInt32>(state_cache.framebuffer_width), static_cast<UInt32>(state_cache.framebuffer_height) };
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state_cache.graphics_pipeline);
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);
	// Flush any deferred descriptor writes before binding
	if (state_cache.srb)
		state_cache.srb->FlushDescriptorWrites();
	if(state_cache.descriptor_sets != nullptr)
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state_cache.pipeline_layout, state_cache.first_set, state_cache.descriptor_sets_count, state_cache.descriptor_sets, 0, nullptr);
	vkCmdDraw(command_buffer, draw_attr.vertexCount, draw_attr.instanceCount, draw_attr.firstVertex, draw_attr.firstInstance);
}

void VK_CommandBuffer::SetVertexBuffer(Buffer* buffer, UInt32 slot, UInt32 stride, UInt32 offset)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdSetVertexBuffer>(buffer, slot, stride, offset)); return; }
	VK_Buffer* vk_buf = STATIC_CAST(buffer, VK_Buffer);
	VkBuffer vkb = vk_buf->GetBuffer();
	VkDeviceSize off = vk_buf->GetOffset() + offset;
	vkCmdBindVertexBuffers(command_buffer, slot, 1, &vkb, &off);
}

void VK_CommandBuffer::SetIndexBuffer(Buffer* buffer, UInt32 offset, Bool index32)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdSetIndexBuffer>(buffer, offset, index32)); return; }
	VK_Buffer* vk_buf = STATIC_CAST(buffer, VK_Buffer);
	vkCmdBindIndexBuffer(command_buffer, vk_buf->GetBuffer(), vk_buf->GetOffset() + offset, index32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
}

void VK_CommandBuffer::DrawIndexed(UInt32 indexCount, UInt32 instanceCount, UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdDrawIndexed>(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance)); return; }
	VkViewport viewport{};
	viewport.x = 0.0f; viewport.y = 0.0f;
	viewport.width = state_cache.framebuffer_width;
	viewport.height = state_cache.framebuffer_height;
	viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { static_cast<UInt32>(state_cache.framebuffer_width), static_cast<UInt32>(state_cache.framebuffer_height) };
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state_cache.graphics_pipeline);
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);
	if (state_cache.srb) state_cache.srb->FlushDescriptorWrites();
	if (state_cache.descriptor_sets != nullptr)
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state_cache.pipeline_layout, state_cache.first_set, state_cache.descriptor_sets_count, state_cache.descriptor_sets, 0, nullptr);
	vkCmdDrawIndexed(command_buffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void VK_CommandBuffer::DrawIndirect(Buffer* args_buffer, UInt32 args_offset, UInt32 draw_count, UInt32 stride)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdDrawIndirect>(args_buffer, args_offset, draw_count, stride)); return; }
	VkViewport viewport{};
	viewport.x = 0.0f; viewport.y = 0.0f;
	viewport.width = state_cache.framebuffer_width;
	viewport.height = state_cache.framebuffer_height;
	viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { static_cast<UInt32>(state_cache.framebuffer_width), static_cast<UInt32>(state_cache.framebuffer_height) };
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state_cache.graphics_pipeline);
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);
	if (state_cache.srb) state_cache.srb->FlushDescriptorWrites();
	if (state_cache.descriptor_sets != nullptr)
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state_cache.pipeline_layout, state_cache.first_set, state_cache.descriptor_sets_count, state_cache.descriptor_sets, 0, nullptr);
	// GetOffset(): sub-allocation offset within the pooled VkBuffer (see SetVertexBuffer)
	VK_Buffer* vk_buf = STATIC_CAST(args_buffer, VK_Buffer);
	vkCmdDrawIndirect(command_buffer, vk_buf->GetBuffer(), (VkDeviceSize)vk_buf->GetOffset() + args_offset, draw_count, stride);
}

void VK_CommandBuffer::DrawIndexedIndirect(Buffer* args_buffer, UInt32 args_offset, UInt32 draw_count, UInt32 stride)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdDrawIndexedIndirect>(args_buffer, args_offset, draw_count, stride)); return; }
	VkViewport viewport{};
	viewport.x = 0.0f; viewport.y = 0.0f;
	viewport.width = state_cache.framebuffer_width;
	viewport.height = state_cache.framebuffer_height;
	viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command_buffer, 0, 1, &viewport);
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { static_cast<UInt32>(state_cache.framebuffer_width), static_cast<UInt32>(state_cache.framebuffer_height) };
	vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state_cache.graphics_pipeline);
	vkCmdSetScissor(command_buffer, 0, 1, &scissor);
	if (state_cache.srb) state_cache.srb->FlushDescriptorWrites();
	if (state_cache.descriptor_sets != nullptr)
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state_cache.pipeline_layout, state_cache.first_set, state_cache.descriptor_sets_count, state_cache.descriptor_sets, 0, nullptr);
	VK_Buffer* vk_buf = STATIC_CAST(args_buffer, VK_Buffer);
	vkCmdDrawIndexedIndirect(command_buffer, vk_buf->GetBuffer(), (VkDeviceSize)vk_buf->GetOffset() + args_offset, draw_count, stride);
}

void VK_CommandBuffer::DispatchIndirect(Buffer* args_buffer, UInt32 args_offset)
{
	if (!bypass) { recorded_commands.push_back(std::make_unique<RHICmdDispatchIndirect>(args_buffer, args_offset)); return; }
	// Compute dispatches must happen outside an active render pass
	if (state_cache.render_pass != VK_NULL_HANDLE)
	{
		EndRenderPass();
	}
	FlushBarriers();
	// Flush any deferred descriptor writes before binding
	if (state_cache.srb)
		state_cache.srb->FlushDescriptorWrites();
	if (state_cache.compute_pipeline != VK_NULL_HANDLE)
		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, state_cache.compute_pipeline);
	if (state_cache.descriptor_sets != nullptr)
		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, state_cache.pipeline_layout, state_cache.first_set, state_cache.descriptor_sets_count, state_cache.descriptor_sets, 0, nullptr);
	VK_Buffer* vk_buf = STATIC_CAST(args_buffer, VK_Buffer);
	vkCmdDispatchIndirect(command_buffer, vk_buf->GetBuffer(), (VkDeviceSize)vk_buf->GetOffset() + args_offset);
}

VK_CommandBufferManager::~VK_CommandBufferManager()
{
	for (UInt32 index = 0; index < graphic_cmd_buffers.size(); ++index)
	{
		delete graphic_cmd_buffers[index];
	}
	for (UInt32 index = 0; index < compute_cmd_buffers.size(); ++index)
	{
		delete compute_cmd_buffers[index];
	}
	for (UInt32 index = 0; index < transfer_cmd_buffers.size(); ++index)
	{
		delete transfer_cmd_buffers[index];
	}
}

VK_CommandBufferManager::VK_CommandBufferManager(VK_Device* in_device):device(in_device)
{
	graphic_cmd_pools.emplace_back(in_device,*this);
	graphic_cmd_pools[0].Init(in_device->GetQueueFamilyIndices().graphics_family.value());
	compute_cmd_pools.emplace_back(in_device, *this);
	compute_cmd_pools[0].Init(in_device->GetQueueFamilyIndices().compute_family.value());
	transfer_cmd_pools.emplace_back(in_device, *this);
	transfer_cmd_pools[0].Init(in_device->GetQueueFamilyIndices().transfer_family.value());

	//graphic_cmd_buffers.push_back(std::move(graphic_cmd_pools[0].GetOrCreateCommandBuffer(false)));
	//compute_cmd_buffers.push_back(std::move(compute_cmd_pools[0].GetOrCreateCommandBuffer(false)));
	//transfer_cmd_buffers.push_back(std::move(transfer_cmd_pools[0].GetOrCreateCommandBuffer(false)));

}

VK_CommandBuffer* VK_CommandBufferManager::GetOrCreateCommandBuffer(ENUM_QUEUE_TYPE queue_type,Bool is_upload_only)
{
	switch (queue_type)
	{
	case ENUM_QUEUE_TYPE::GRAPHICS :
		return graphic_cmd_pools[0].GetOrCreateCommandBuffer(is_upload_only);
	case ENUM_QUEUE_TYPE::COMPUTE:
		return compute_cmd_pools[0].GetOrCreateCommandBuffer(is_upload_only);
	case ENUM_QUEUE_TYPE::TRANSFER:
		return transfer_cmd_pools[0].GetOrCreateCommandBuffer(is_upload_only);
	default:
		return nullptr;
	}
}

void VK_CommandBufferManager::ReleaseCommandBuffer(VK_CommandBuffer* cmd_buffer)
{
	//cmd_buffer->GetFence()->ResetFence();
	cmd_buffer->owner_pool->FreeUnusedCommandBuffer(cmd_buffer);
}

void VK_CommandBufferManager::FreeUnusedCommandBuffer(ENUM_QUEUE_TYPE queue_type)
{
	switch (queue_type)
	{
	case ENUM_QUEUE_TYPE::GRAPHICS:
		return graphic_cmd_pools[0].FreeUnusedCommandBuffers(nullptr);
	case ENUM_QUEUE_TYPE::COMPUTE:
		return compute_cmd_pools[0].FreeUnusedCommandBuffers(nullptr);
	case ENUM_QUEUE_TYPE::TRANSFER:
		return transfer_cmd_pools[0].FreeUnusedCommandBuffers(nullptr);
	default:
		return ;
	}
}

void VK_CommandBuffer::Replay()
{
	if (recorded_commands.empty()) return;

	Bool saved_bypass = bypass;
	bypass = true;  // During replay, execute vk calls directly

	for (auto& cmd : recorded_commands)
	{
		switch (cmd->type)
		{
		case RHICommandType::SetGraphicsPipeline:
			SetGraphicsPipeline(static_cast<RHICmdSetGraphicsPipeline*>(cmd.get())->pso);
			break;
		case RHICommandType::SetComputePipeline:
			SetComputePipeline(static_cast<RHICmdSetComputePipeline*>(cmd.get())->pso);
			break;
		case RHICommandType::SetRenderTarget: {
			auto* c = static_cast<RHICmdSetRenderTarget*>(cmd.get());
			SetRenderTarget(c->rtvs, c->dsv, c->clear_values, c->has_dsv_clear);
			break;
		}
		case RHICommandType::SetSRB:
			SetShaderResourceBinding(static_cast<RHICmdSetSRB*>(cmd.get())->srb);
			break;
		case RHICommandType::Draw:
			Draw(static_cast<RHICmdDraw*>(cmd.get())->attr);
			break;
		case RHICommandType::SetVertexBuffer: {
			auto* c = static_cast<RHICmdSetVertexBuffer*>(cmd.get());
			SetVertexBuffer(c->buffer, c->slot, c->stride, c->offset);
			break;
		}
		case RHICommandType::SetIndexBuffer: {
			auto* c = static_cast<RHICmdSetIndexBuffer*>(cmd.get());
			SetIndexBuffer(c->buffer, c->offset, c->index32);
			break;
		}
		case RHICommandType::DrawIndexed: {
			auto* c = static_cast<RHICmdDrawIndexed*>(cmd.get());
			DrawIndexed(c->indexCount, c->instanceCount, c->firstIndex, c->vertexOffset, c->firstInstance);
			break;
		}
		case RHICommandType::DrawIndirect: {
			auto* c = static_cast<RHICmdDrawIndirect*>(cmd.get());
			DrawIndirect(c->buffer, c->offset, c->draw_count, c->stride);
			break;
		}
		case RHICommandType::DrawIndexedIndirect: {
			auto* c = static_cast<RHICmdDrawIndexedIndirect*>(cmd.get());
			DrawIndexedIndirect(c->buffer, c->offset, c->draw_count, c->stride);
			break;
		}
		case RHICommandType::Dispatch: {
			auto* c = static_cast<RHICmdDispatch*>(cmd.get());
			Dispatch(c->gx, c->gy, c->gz);
			break;
		}
		case RHICommandType::DispatchIndirect: {
			auto* c = static_cast<RHICmdDispatchIndirect*>(cmd.get());
			DispatchIndirect(c->buffer, c->offset);
			break;
		}
		case RHICommandType::TransitionTexture: {
			auto* c = static_cast<RHICmdTransitionTexture*>(cmd.get());
			TransitionTextureState(c->texture, c->state);
			break;
		}
		case RHICommandType::ClearTexture: {
			auto* c = static_cast<RHICmdClearTexture*>(cmd.get());
			ClearTexture(c->texture, c->clear_value);
			break;
		}
		case RHICommandType::ResourceBarrier: {
			auto* c = static_cast<RHICmdResourceBarrier*>(cmd.get());
			ResourceBarrier(c->src, c->dst);
			break;
		}
				case RHICommandType::FlushBarriers:
			FlushBarriers();
			break;
		case RHICommandType::SetPushConstants: {
			auto* c = static_cast<RHICmdSetPushConstants*>(cmd.get());
			if (c->stage != ENUM_SHADER_STAGE::Invalid)
				SetPushConstants(c->offset, c->size, c->data.data(), c->stage);
			else
				SetPushConstants(c->offset, c->size, c->data.data());
			break;
		}
		case RHICommandType::Begin:
			Begin();
			break;
		case RHICommandType::End:
			End();
			break;
		case RHICommandType::BeginRenderPass: {
			auto* c = static_cast<RHICmdBeginRenderPass*>(cmd.get());
			BeginRenderPass(
				(VkRenderPass)(uintptr_t)c->render_pass,
				(VkFramebuffer)(uintptr_t)c->frame_buffer,
				c->width, c->height, c->clear_count,
				c->clear_count > 0 ? (const VkClearValue*)c->clear_value_data.data() : nullptr);
			break;
		}
		case RHICommandType::EndRenderPass:
			EndRenderPass();
			break;
		case RHICommandType::BeginUI:
			// CPU phase already executed on main thread (NewFrame)
			break;
		case RHICommandType::SetScissorEnable: {
			auto* c = static_cast<RHICmdSetScissorEnable*>(cmd.get());
			if (c->enable)
				vkCmdSetScissor(command_buffer, 0, 1, &state_cache.scissor);
			else {
				VkRect2D full = { {0, 0}, {state_cache.framebuffer_width, state_cache.framebuffer_height} };
				vkCmdSetScissor(command_buffer, 0, 1, &full);
			}
			state_cache.scissor_enabled = c->enable;
			break;
		}
		case RHICommandType::SetScissor: {
			auto* c = static_cast<RHICmdSetScissor*>(cmd.get());
			state_cache.scissor.offset.x = c->x;
			state_cache.scissor.offset.y = c->y;
			state_cache.scissor.extent.width = c->w;
			state_cache.scissor.extent.height = c->h;
			vkCmdSetScissor(command_buffer, 0, 1, &state_cache.scissor);
			break;
		}
#if !PLATFORM_ANDROID
		case RHICommandType::EndUI:
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
			break;
		case RHICommandType::RenderImGui: {
			auto* c = static_cast<RHICmdRenderImGui*>(cmd.get());
			auto* prev_ctx = ImGui::GetCurrentContext();
			if (c->imgui_context)
				ImGui::SetCurrentContext(static_cast<ImGuiContext*>(c->imgui_context));
			if (c->draw_data)
				ImGui_ImplVulkan_RenderDrawData(static_cast<ImDrawData*>(c->draw_data), command_buffer);
			ImGui::SetCurrentContext(prev_ctx);
			break;
		}
#endif
		case RHICommandType::WriteTimestamp: {
			auto* c = static_cast<RHICmdWriteTimestamp*>(cmd.get());
			WriteTimestamp(c->index);
			break;
		}
				case RHICommandType::CopyBuffer:
		{
			auto* cc = static_cast<RHICmdCopyBuffer*>(cmd.get());
			if (cc->region_count > 0 && !cc->regions.empty())
				CopyBuffer((VkBuffer)(uintptr_t)cc->src_id, (VkBuffer)(uintptr_t)cc->dst_id, cc->region_count, (const VkBufferCopy*)cc->regions.data());
			break;
		}
		case RHICommandType::CopyBufferToImage: {
			auto* cc = static_cast<RHICmdCopyBufferToImage*>(cmd.get());
			if (cc->region_count > 0 && !cc->regions.empty())
				CopyBufferToImage((VkBuffer)(uintptr_t)cc->src_buffer, (VkImage)(uintptr_t)cc->dst_image, (VkImageLayout)cc->image_layout, cc->region_count, (const VkBufferImageCopy*)cc->regions.data());
			break;
		}
		case RHICommandType::UnmapBuffer: {
			auto* c = static_cast<RHICmdUnmapBuffer*>(cmd.get());
			VK_Buffer* vk_buf = STATIC_CAST(c->buffer, VK_Buffer);
			if (vk_buf) vk_buf->Unmap();
			break;
		}
		case RHICommandType::UpdateBuffer: {
			auto* c = static_cast<RHICmdUpdateBuffer*>(cmd.get());
			VK_Buffer* vk_buf = STATIC_CAST(c->buffer, VK_Buffer);
			if (vk_buf) {
				UInt32 buf_offset = vk_buf->GetOffset() + c->offset;
				vkCmdUpdateBuffer(command_buffer, vk_buf->GetBuffer(),
					buf_offset, c->size, c->data.data());
				VkBufferMemoryBarrier barrier{};
				barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				barrier.buffer = vk_buf->GetBuffer();
				barrier.offset = buf_offset;
				barrier.size = c->size;
				vkCmdPipelineBarrier(command_buffer,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
					VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					0, 0, nullptr, 1, &barrier, 0, nullptr);
			}
			break;
		}
		default:
			break;
		}
	}

	bypass = saved_bypass;
	recorded_commands.clear();
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
