#pragma once
#ifndef _VK_COMMANDBUFFER_
#define _VK_COMMANDBUFFER_
#include <vulkan/vulkan_core.h>
#include "RHI/RenderCommandList.h"
#include "VK_Fence.h"
#include "VK_Device.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

class VK_Device;
class VK_CommandBuffer; 
class VK_CommandBufferPool;
class VK_CommandBufferManager;
class VK_Fence;
class VK_ShaderResourceBinding;
class VK_Queue;
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VK_CommandBufferPool,public RenderResource)
#pragma region METHOD
public:
	VK_CommandBufferPool(VK_Device* in_device, VK_CommandBufferManager& in_cmd_buffer_manager);
	VIRTUAL ~VK_CommandBufferPool();

	VK_CommandBuffer* METHOD(GetOrCreateCommandBuffer)(Bool is_upload_only);

	void METHOD(FreeUnusedCommandBuffers)(VK_Queue* queue);
	void METHOD(FreeUnusedCommandBuffer)(VK_CommandBuffer* target);
	void METHOD(Init)(UInt32 queue_family_index);
	VkCommandPool METHOD(GetPool)() CONST;
protected:

private:

#pragma endregion

#pragma region MEMBER
public:

protected:
	VkCommandPool command_pool = VK_NULL_HANDLE;

	Vector<VK_CommandBuffer*> cmd_buffers;
	Vector<VK_CommandBuffer*> free_cmd_buffers;

	VK_Device* device = nullptr;

	VK_CommandBufferManager& cmd_buffer_manager;
private:
	
#pragma endregion
MYRENDERER_END_CLASS



MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VK_CommandBuffer, public CommandList)
friend VK_CommandBufferPool;
friend VK_Queue;
friend VK_CommandBufferManager;
	friend class VulkanRHI; // --  
#pragma region METHOD
public:
	VK_CommandBuffer(VK_Device* in_device, VK_CommandBufferPool* in_command_buffer_pool, Bool in_is_upload_only);
	VIRTUAL ~VK_CommandBuffer();

	VkCommandBuffer METHOD(GetCommandBuffer)() CONST;
	VK_Fence* METHOD(GetFence)() CONST;
	VIRTUAL Bool METHOD(WaitForFence)(float time_in_seconds_to_wait) OVERRIDE FINAL;

	UInt64 METHOD(GetFenceSignaledCounter)() CONST;

	void  METHOD(TrainsitionImageLayout)(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, CONST VkImageSubresourceRange& subresourceRange, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);
	void  METHOD(FlushBarriers)();
	void  METHOD(MemoryBarrier)(VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
	void  METHOD(CopyBufferToImage)(VkBuffer buffer, VkImage image, VkImageLayout imageLayout, UInt32 region_count, CONST VkBufferImageCopy* region);
	
	__forceinline void CopyBuffer(VkBuffer src_buffer,VkBuffer dst_buffer,uint32_t region_count,const VkBufferCopy* regions)
	{
		if(!bypass){recorded_commands.push_back(std::make_unique<RHICmdCopyBuffer>((UInt64)(uintptr_t)src_buffer,(UInt64)(uintptr_t)dst_buffer,region_count,regions,region_count*sizeof(VkBufferCopy)));return;}
		if (state_cache.render_pass != VK_NULL_HANDLE)
		{
			EndRenderPass();
		}
		FlushBarriers();
		vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, region_count, regions);
	}
	__forceinline void METHOD(BeginRenderPass)(VkRenderPass in_render_pass,
		VkFramebuffer       in_frame_buffer,
		UInt32            in_framebuffer_width,
		UInt32            in_framebuffer_height,
		UInt32            in_clear_value_count = 0,
		CONST VkClearValue* in_clear_values = nullptr)
	{
		if(!bypass){FlushBarriers();recorded_commands.push_back(std::make_unique<RHICmdBeginRenderPass>((UInt64)(uintptr_t)in_render_pass,(UInt64)(uintptr_t)in_frame_buffer,in_framebuffer_width,in_framebuffer_height,in_clear_value_count,in_clear_values));return;}
		if (state_cache.render_pass != in_render_pass || state_cache.framebuffer != in_frame_buffer)
		{
			FlushBarriers();

			VkRenderPassBeginInfo begin_info;
			begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			begin_info.pNext = nullptr;
			begin_info.renderPass = in_render_pass;
			begin_info.framebuffer = in_frame_buffer;
			
			begin_info.renderArea = { {0, 0}, {in_framebuffer_width, in_framebuffer_height} };
			begin_info.clearValueCount = in_clear_value_count;
			begin_info.pClearValues = in_clear_values; // an array of VkClearValue structures that contains clear values for
			// each attachment, if the attachment uses a loadOp value of VK_ATTACHMENT_LOAD_OP_CLEAR
			// or if the attachment has a depth/stencil format and uses a stencilLoadOp value of
			// VK_ATTACHMENT_LOAD_OP_CLEAR. The array is indexed by attachment number. Only elements
			// corresponding to cleared attachments are used. Other elements of pClearValues are
			// ignored 

			vkCmdBeginRenderPass(command_buffer, &begin_info,
				VK_SUBPASS_CONTENTS_INLINE // the contents of the subpass will be recorded inline in the
										   // primary command buffer, and secondary command buffers must not
										   // be executed within the subpass
			);
			state_cache.render_pass = in_render_pass;
			state_cache.framebuffer = in_frame_buffer;
			state_cache.framebuffer_width = in_framebuffer_width;
			state_cache.framebuffer_height = in_framebuffer_height;
			state_cache.clear_values = (VkClearValue*)in_clear_values;
			state_cache.clear_value_count = in_clear_value_count;
			command_state = EState::IsInsideRenderPass;
		}
	}

	__forceinline void METHOD(EndRenderPass)()
	{
		if(!bypass)return;
		if (command_state == EState::IsInsideRenderPass)
		{
			vkCmdEndRenderPass(command_buffer);
			state_cache = StateCache();
			command_state = EState::HasEndedRenderPass;
		}
	}

	__forceinline void METHOD(BeginDynamicRendering)(CONST Vector<Texture*>& render_targets, Texture* depth_stencil, UInt32 width, UInt32 height, UInt32 clear_value_count, CONST VkClearValue* clear_values);
	__forceinline void METHOD(EndDynamicRendering)()
	{
		if(!bypass)return;
		if (command_state == EState::IsInsideRenderPass)
		{
			vkCmdEndRendering(command_buffer);
			state_cache = StateCache();
			command_state = EState::HasEndedRenderPass;
		}
	}

	__forceinline VIRTUAL void METHOD(Begin)() OVERRIDE FINAL
	{
		if(!bypass){recorded_commands.push_back(std::make_unique<RHICmdBegin>());return;}
		if (command_state == EState::NeedReset || command_state == EState::Submitted)
		{
			// Ensure GPU has finished with this CB before resetting.
			// CheckCompletion no longer resets fences; Begin() is the sole point
			// where the fence is reset for the next submit cycle.
			// MUST keep waiting on timeout: resetting a command buffer that is
			// still executing on the GPU is undefined behavior (observed as a
			// permanent GPU hang under heavy per-frame workloads).
			while (!fence->CheckSignaled())
			{
				device->GetFenceManager()->WaitForFence(fence, 1000000000ULL); // 1s slices
			}
			fence->ResetFence(); // signaled -> unsignaled (required for next vkQueueSubmit)
			vkResetCommandBuffer(command_buffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
			command_state = EState::ReadyForBegin;
		}
		if (command_state != EState::IsInsideBegin && command_state != EState::IsInsideRenderPass && command_state != EState::HasEndedRenderPass)
		{
			VkCommandBufferBeginInfo begin_info{};
			begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin_info.pNext = nullptr;
			CHECK_WITH_LOG(vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS, "RHI Error : fail to begion commandbuffer ");
			command_state = EState::IsInsideBegin;
		}
	}
	__forceinline VIRTUAL void METHOD(End)() OVERRIDE FINAL
	{
		if(!bypass){FlushBarriers();recorded_commands.push_back(std::make_unique<RHICmdEnd>());return;}
		if (command_state > EState::NeedReset && command_state < EState::HasEndedCommandBuffer)
		{
			// Distinguish legacy render pass vs dynamic rendering by state_cache.render_pass.
			// Legacy BeginRenderPass sets it non-null; BeginDynamicRendering leaves it VK_NULL_HANDLE.
			if (state_cache.render_pass != VK_NULL_HANDLE)
				EndRenderPass();
			else
				EndDynamicRendering();
			FlushBarriers();
			CHECK_WITH_LOG(vkEndCommandBuffer(command_buffer) != VK_SUCCESS, "RHI Error : fail to end commandbuffer ");
			command_state = EState::NeedReset;
		}
	}
	__forceinline void METHOD(ClearAttachment)(CONST VkClearAttachment& attachment, CONST VkClearRect& clear_rect)
	{
		if(!bypass)return;
		vkCmdClearAttachments(
			command_buffer,
			1,
			&attachment,
			1,
			&clear_rect // The rectangular region specified by each element of pRects must be
			// contained within the render area of the current render pass instance
		);
	}

	__forceinline void ClearDepthStencilImage(VkImage image,CONST VkClearDepthStencilValue& depth_stencil,CONST VkImageSubresourceRange& subresource)
	{
		if(!bypass)return;

		FlushBarriers();
		vkCmdClearDepthStencilImage(
			command_buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			&depth_stencil,
			1,
			&subresource);
	}

	__forceinline void ClearColorImage(VkImage  image, CONST VkClearColorValue& color,CONST VkImageSubresourceRange& subresource)
	{
		if(!bypass)return;
		FlushBarriers();
		vkCmdClearColorImage(
			command_buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			&color,
			1,
			&subresource);
	}

	__forceinline VIRTUAL void METHOD(SetGraphicsPipeline)(RenderPipelineState* pipeline_state) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(SetComputePipeline)(RenderPipelineState* pipeline_state) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(SetRenderTarget)(CONST Vector<Texture*>& render_targets, Texture* depth_stencil, CONST Vector<ClearValue>& clear_values, Bool has_dsv_clear_value) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(SetShaderResourceBinding)(ShaderResourceBinding* srb) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(Draw)(CONST DrawAttribute& draw_attr) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(SetVertexBuffer)(Buffer* buffer, UInt32 slot, UInt32 stride, UInt32 offset) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(SetIndexBuffer)(Buffer* buffer, UInt32 offset, Bool index32) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(DrawIndexed)(UInt32 indexCount, UInt32 instanceCount, UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(DrawIndirect)(Buffer* args_buffer, UInt32 args_offset, UInt32 draw_count, UInt32 stride = (UInt32)sizeof(DrawIndirectArgs)) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(DrawIndexedIndirect)(Buffer* args_buffer, UInt32 args_offset, UInt32 draw_count, UInt32 stride = (UInt32)sizeof(DrawIndexedIndirectArgs)) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(Dispatch)(UInt32 groupX, UInt32 groupY, UInt32 groupZ) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(DispatchIndirect)(Buffer* args_buffer, UInt32 args_offset) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(SetPushConstants)(UInt32 offset, UInt32 size, const void* data) OVERRIDE FINAL;
	// --   Stage-aware push constants overload
	__forceinline VIRTUAL void METHOD(SetPushConstants)(UInt32 offset, UInt32 size, const void* data, ENUM_SHADER_STAGE stage) OVERRIDE FINAL;
	// --   RHI barrier + compute dispatch overrides
	__forceinline VIRTUAL void METHOD(ResourceBarrier)(ENUM_RESOURCE_STATE src_state, ENUM_RESOURCE_STATE dst_state) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(MemoryBarrier)(ENUM_SHADER_STAGE src_stage, ENUM_SHADER_STAGE dst_stage, ENUM_RESOURCE_STATE src_access, ENUM_RESOURCE_STATE dst_access) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(ComputeDispatch)(RenderPipelineState* pipeline, ShaderResourceBinding* srb, UInt32 groupX, UInt32 groupY, UInt32 groupZ) OVERRIDE FINAL;
	// --  
	__forceinline VIRTUAL void METHOD(TransitionTextureState)(Texture* texture, CONST ENUM_RESOURCE_STATE& required_state) OVERRIDE FINAL;
	__forceinline VIRTUAL void METHOD(ClearTexture)(Texture* texture, Vector<float> clear_value = Vector<float>(4, 0.0f)) OVERRIDE FINAL;
	VIRTUAL void METHOD(Replay)() OVERRIDE FINAL;

	__forceinline VIRTUAL void METHOD(BeginUI)()  OVERRIDE FINAL;

	__forceinline VIRTUAL void METHOD(EndUI)()  OVERRIDE FINAL;
	//  三线程模式：拆分 UI 阶段
	VIRTUAL void METHOD(BeginUI_Logic)() ;
	VIRTUAL void METHOD(EndUI_Render)() ;
	VIRTUAL void METHOD(EndUI_Platform)() ;
	// Dynamic scissor state — RHI abstract interface override
	VIRTUAL void METHOD(SetScissorEnable)(bool enable) OVERRIDE FINAL;
	VIRTUAL void METHOD(SetScissor)(Int x, Int y, UInt32 w, UInt32 h) OVERRIDE FINAL;
protected:
	void METHOD(Allocate)();
	void METHOD(Free)();

	void METHOD(TransitionRenderTargets)(CONST Vector<Texture*>& render_targets, Texture* depth_stencil);
private:

#pragma endregion

#pragma region MEMBER
public:
	MYRENDERER_BEGIN_STRUCT( StateCache)
	public:
		VkRenderPass  render_pass = VK_NULL_HANDLE;
		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		VkPipeline    graphics_pipeline = VK_NULL_HANDLE;
		VkPipeline    compute_pipeline = VK_NULL_HANDLE;
		VkPipeline    raytracing_pipeline = VK_NULL_HANDLE;
		VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
		UInt8         descriptor_sets_count = 0;
		UInt32        first_set = 0;
		CONST VkDescriptorSet* descriptor_sets = nullptr;
		VK_ShaderResourceBinding* srb = nullptr;
		VkBuffer      index_buffer = VK_NULL_HANDLE;
		VkDeviceSize  index_buffer_offset = 0;
		VkIndexType   index_type = VK_INDEX_TYPE_MAX_ENUM;
		UInt32      framebuffer_width = 0;
		UInt32      framebuffer_height = 0;
		UInt32      inside_pass_queries = 0;
		UInt32      outside_pass_queries = 0;
		VkClearValue* clear_values = nullptr;
		UInt32      clear_value_count = 0;
		//  Scissor state (for UI rendering)
		bool        scissor_enabled = false;
		VkRect2D    scissor = { {0, 0}, {0xFFFFFFFF, 0xFFFFFFFF} };

		Vector<Texture*> render_targets;
		Texture* depth_stencil = nullptr;
	MYRENDERER_END_STRUCT


	enum class EState : UInt8
	{
		ReadyForBegin = 0,
		NeedReset,
		IsInsideBegin,
		IsInsideRenderPass,
		HasEndedRenderPass,
		HasEndedCommandBuffer,
		Submitted,
		NotAllocated,
	};

	EState command_state = EState::NotAllocated;
protected:
	VK_Device* device;
	VkCommandBuffer command_buffer;
	VK_CommandBufferPool* owner_pool;
	Vector<VkSemaphore*> wait_semaphores;
	Vector<VkSemaphore*> submitted_wait_semaphores;
	VK_Fence* fence=nullptr;
	Bool is_upload_only=false;


	volatile UInt64 fence_signaled_counter;
	volatile UInt64 submitted_fence_counter;

	StateCache state_cache;

	MYRENDERER_BEGIN_STRUCT(PipelineBarrier)
	public:
		VkPipelineStageFlags memory_src_stages = 0;
		VkPipelineStageFlags memory_dst_stages = 0;
		VkAccessFlags        memory_src_access = 0;
		VkAccessFlags        memory_dst_access = 0;

		VkPipelineStageFlags image_src_stages = 0;
		VkPipelineStageFlags image_dst_stages = 0;

		VkPipelineStageFlags supported_stages_mask = ~0u;
		VkAccessFlags        supported_access_mask = ~0u;
	MYRENDERER_END_STRUCT

	PipelineBarrier pipeline_barrier;

	Vector<VkImageMemoryBarrier> image_barriers;

private:

#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VK_CommandBufferManager, public RenderResource)
#pragma region METHOD
public:

VK_CommandBufferManager(VK_Device* in_device );
VIRTUAL ~VK_CommandBufferManager();

VK_CommandBuffer* METHOD(GetOrCreateCommandBuffer)(ENUM_QUEUE_TYPE queue_type, Bool is_upload_only=false);
void METHOD(ReleaseCommandBuffer)(VK_CommandBuffer* cmd_buffer);
void METHOD(FreeUnusedCommandBuffer)(ENUM_QUEUE_TYPE queue_type);
protected:

private:

#pragma endregion

#pragma region MEMBER
public:

protected:
	VK_Device* device;
	Vector<VK_CommandBuffer*> graphic_cmd_buffers;
	Vector<VK_CommandBuffer*> compute_cmd_buffers;
	Vector<VK_CommandBuffer*> transfer_cmd_buffers;
	Vector<VK_CommandBuffer*> present_cmd_buffers;
	Vector<VK_CommandBufferPool> graphic_cmd_pools;
	Vector<VK_CommandBufferPool> compute_cmd_pools;
	Vector<VK_CommandBufferPool> transfer_cmd_pools;
	Vector<VK_CommandBufferPool> present_cmd_pools;

	Vector<VK_CommandBuffer*> using_graphic_cmd_buffers;
	Vector<VK_CommandBuffer*> using_compute_cmd_buffers;
	Vector<VK_CommandBuffer*> using_transfer_cmd_buffers;
	Vector<VK_CommandBuffer*> using_present_cmd_buffers;
private:

#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif 
