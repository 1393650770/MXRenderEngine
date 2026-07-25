#include "RenderRHI.h"
#include "Platform/Platform.h"
#if !PLATFORM_GLES3 && !PLATFORM_WGPU
#include "Vulkan/VK_RenderPass.h"
#include "Vulkan/VK_RenderRHI.h"
#include "Vulkan/VK_BindlessManager.h"
#endif
#include "ResourceManager.h"
#include "RenderTexture.h"

MXRender::RHI::RenderRHI* g_render_rhi = nullptr;
Bool g_enable_rhi_thread = false;

void RHIInit()
{
	if (g_render_rhi == nullptr)
	{
		g_render_rhi = PlatformCreateDynamicRHI();
	}
}

void RHIShutdown()
{


	if (g_render_rhi != nullptr)
	{
		g_render_rhi->Shutdown();
		delete g_render_rhi;
		g_render_rhi = nullptr;
	}
}

MXRender::RHI::Viewport* RHICreateViewport(void* window_handle, Int width, Int height, Bool is_full_screen)
{
	return g_render_rhi->CreateViewport(window_handle, width, height, is_full_screen);
}

MXRender::RHI::TextureHandle RHICreateTexture(CONST MXRender::RHI::TextureDesc& texture_desc, CONST String& name)
{
	if (MXRender::RHI::g_resource_manager)
		return Create<MXRender::RHI::TextureHandle>(texture_desc, name);
	MXRender::RHI::Texture* raw = g_render_rhi->CreateTexture(texture_desc);
	return MXRender::RHI::TextureHandle{};
}

MXRender::RHI::BufferHandle RHICreateBuffer(CONST MXRender::RHI::BufferDesc& buffer_desc, CONST String& name)
{
	if (MXRender::RHI::g_resource_manager)
		return Create<MXRender::RHI::BufferHandle>(buffer_desc, name);
	MXRender::RHI::Buffer* raw = g_render_rhi->CreateBuffer(buffer_desc);
	return MXRender::RHI::BufferHandle{};
}

MXRender::RHI::Shader* RHICreateShader(CONST MXRender::RHI::ShaderDesc& desc, CONST MXRender::RHI::ShaderDataPayload& data)
{
	return g_render_rhi->CreateShader(desc,data);
}

MXRender::RHI::PSOHandle RHICreateRenderPipelineState(CONST MXRender::RHI::RenderGraphiPipelineStateDesc& desc, CONST String& name)
{
	if (MXRender::RHI::g_resource_manager)
		return Create<MXRender::RHI::PSOHandle>(desc, name);
	MXRender::RHI::RenderPipelineState* raw = g_render_rhi->CreateRenderPipelineState(desc);
	return MXRender::RHI::PSOHandle{};
}

// --  
MXRender::RHI::ComputePipelineState* RHICreateComputePipelineState(CONST MXRender::RHI::ComputePipelineStateDesc& desc)
{
	return g_render_rhi->CreateComputePipelineState(desc);
}

MXRender::RHI::RenderPass* RHICreateRenderPass(CONST MXRender::RHI::RenderPassDesc& desc)
{
	return g_render_rhi->CreateRenderPass(desc);
}

MXRender::RHI::FrameBuffer* RHICreateFrameBuffer(CONST MXRender::RHI::FrameBufferDesc& desc)
{
	return g_render_rhi->CreateFrameBuffer(desc);
}

MXRender::RHI::CommandList* RHIGetImmediateCommandList()
{
 	return g_render_rhi->GetImmediateCommandList();
}

// --  
MXRender::RHI::CommandList* RHIGetCommandListForQueue(MXRender::ENUM_QUEUE_TYPE queue_type)
{
	return g_render_rhi->GetCommandListForQueue(queue_type);
}

void RHISubmitCommandList(MXRender::RHI::CommandList* command_list)
{
	g_render_rhi->SubmitCommandList(command_list);
}

// --  
void RHISubmitCommandListForQueue(MXRender::RHI::CommandList* cmd_list, MXRender::ENUM_QUEUE_TYPE queue_type)
{
	g_render_rhi->SubmitCommandListForQueue(cmd_list, queue_type);
}

void RHIRenderEnd()
{
	g_render_rhi->RenderEnd();
}

void* RHIMapBuffer(MXRender::RHI::Buffer* buffer, MXRender::ENUM_MAP_TYPE map_type, MXRender::ENUM_MAP_FLAG map_flag)
{
	return g_render_rhi->MapBuffer(buffer, map_type,map_flag);
}

void RHIUnmapBuffer(MXRender::RHI::Buffer* buffer)
{
	g_render_rhi->UnmapBuffer(buffer);
}

// --   Async RHI thread (lock-free)
MXRender::RHI::CommandList* RHIGetWriteCommandList()
{
	return g_render_rhi->GetWriteCommandList();
}
MXRender::RHI::CommandList* RHIGetRHICmdListForPresent()
{
	return g_render_rhi->GetRHICmdListForPresent();
}
void RHISwapCommandLists()
{
	g_render_rhi->SwapCommandLists();
}
Bool RHIIsReplayDone()
{
	return g_render_rhi->IsReplayDone();
}
void RHIStartRHIThread()
{
	g_render_rhi->StartRHIThread();
}
void RHIStopRHIThread()
{
	g_render_rhi->StopRHIThread();
}

MXRender::RHI::BindlessManager* RHIGetBindlessManager()
{
	return g_render_rhi ? g_render_rhi->GetBindlessManager() : nullptr;
}

