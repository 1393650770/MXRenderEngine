#pragma once
#ifndef _WGPU_RENDERRHI_
#define _WGPU_RENDERRHI_

#if PLATFORM_WGPU

#include "RHI/RenderRHI.h"
#include <webgpu/webgpu.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)

class Viewport;
class Shader;
class Buffer;
class Texture;
class RenderPipelineState;
class ComputePipelineState;
class RenderPass;
class FrameBuffer;
class CommandList;
class BindlessManager;

MYRENDERER_BEGIN_NAMESPACE(WebGPU)

class WGPU_Viewport;
class WGPU_CommandBuffer;
class WGPU_PipelineState;

// WGPU_RenderRHI: WebGPU backend for the RHI abstraction layer.
// Async init: browser WebGPU adapter/device requests are callback-based,
// so Init() kicks off the request and IsReady() must be polled before rendering.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(WGPU_RenderRHI, public RenderRHI)

#pragma region METHOD
public:
	WGPU_RenderRHI() MYDEFAULT;
	VIRTUAL ~WGPU_RenderRHI() MYDEFAULT;

#pragma region INIT_METHOD
	VIRTUAL void METHOD(Init)(RenderFactory* render_factory) OVERRIDE FINAL;
	VIRTUAL void METHOD(PostInit)() OVERRIDE FINAL;
	VIRTUAL void METHOD(Shutdown)() OVERRIDE FINAL;
#pragma endregion

#pragma region CREATE_RESOURCE
	VIRTUAL Viewport* METHOD(CreateViewport)(void* window_handle, Int width, Int height, Bool is_full_screen) OVERRIDE FINAL;
	VIRTUAL Shader* METHOD(CreateShader)(CONST ShaderDesc& desc, CONST ShaderDataPayload& data) OVERRIDE FINAL;
	VIRTUAL Buffer* METHOD(CreateBuffer)(CONST BufferDesc& buffer_desc) OVERRIDE FINAL;
	VIRTUAL Texture* METHOD(CreateTexture)(CONST TextureDesc& texture_desc) OVERRIDE FINAL;
	VIRTUAL RenderPipelineState* METHOD(CreateRenderPipelineState)(CONST RenderGraphiPipelineStateDesc& desc) OVERRIDE FINAL;
	VIRTUAL ComputePipelineState* METHOD(CreateComputePipelineState)(CONST ComputePipelineStateDesc& desc) OVERRIDE FINAL;
	VIRTUAL RenderPass* METHOD(CreateRenderPass)(CONST RenderPassDesc& desc) OVERRIDE FINAL;
	VIRTUAL FrameBuffer* METHOD(CreateFrameBuffer)(CONST FrameBufferDesc& desc) OVERRIDE FINAL;

	VIRTUAL void* METHOD(MapBuffer)(Buffer* buffer, ENUM_MAP_TYPE map_type, ENUM_MAP_FLAG map_flag) OVERRIDE FINAL;
	VIRTUAL void METHOD(UnmapBuffer)(Buffer* buffer) OVERRIDE FINAL;
#pragma endregion

#pragma region DRAW
	VIRTUAL CommandList* METHOD(GetImmediateCommandList)() OVERRIDE FINAL;
	VIRTUAL CommandList* METHOD(GetCommandListForQueue)(ENUM_QUEUE_TYPE queue_type) OVERRIDE FINAL;
	VIRTUAL void METHOD(SubmitCommandList)(CommandList* command_list) OVERRIDE FINAL;
	VIRTUAL void METHOD(SubmitCommandListForQueue)(CommandList* cmd_list, ENUM_QUEUE_TYPE queue_type) OVERRIDE FINAL;
	VIRTUAL void METHOD(RenderEnd)() OVERRIDE FINAL;
	VIRTUAL CommandList* METHOD(GetWriteCommandList)() OVERRIDE FINAL;
	VIRTUAL CommandList* METHOD(GetRHICmdListForPresent)() OVERRIDE FINAL;
	VIRTUAL void METHOD(SwapCommandLists)() OVERRIDE FINAL;
	VIRTUAL Bool METHOD(IsReplayDone)() CONST OVERRIDE FINAL;
	VIRTUAL void METHOD(StartRHIThread)() OVERRIDE FINAL;
	VIRTUAL void METHOD(StopRHIThread)() OVERRIDE FINAL;
	VIRTUAL BindlessManager* METHOD(GetBindlessManager)() OVERRIDE FINAL;
#pragma endregion

	// Async init polling: returns true once adapter/device/queue are ready.
	Bool METHOD(IsReady)() CONST { return m_is_ready; }
	WGPUDevice METHOD(GetDevice)() CONST { return m_device; }
	WGPUQueue METHOD(GetQueue)() CONST { return m_queue; }
	WGPUSurface METHOD(GetSurface)() CONST { return m_surface; }
	WGPUSwapChain METHOD(GetSwapChain)() CONST { return m_swap_chain; }

	void METHOD(ConfigureSwapChain)(UInt32 width, UInt32 height);

protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	WGPUInstance m_instance = nullptr;
	WGPUAdapter m_adapter = nullptr;
	WGPUDevice m_device = nullptr;
	WGPUQueue m_queue = nullptr;
	WGPUSurface m_surface = nullptr;
	WGPUSwapChain m_swap_chain = nullptr;

	Bool m_is_ready = false;
	UInt32 m_swapchain_width = 0;
	UInt32 m_swapchain_height = 0;

	WGPU_CommandBuffer* m_immediate_cmd = nullptr;
	WGPU_Viewport* m_viewport = nullptr;

	// PSO ownership (Decision #8 / §12.12): WGPU_RenderRHI owns all PSOs.
	// External code must NOT delete PSO pointers.
	Vector<WGPU_PipelineState*> m_pso_storage;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
#endif // _WGPU_RENDERRHI_
