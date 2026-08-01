#pragma once

#ifndef _RENDERRHI_
#define _RENDERRHI_
#include"RenderEnum.h"
#include <string>
#include<vector>
#include<memory>
#include "RenderResource.h"
#include "Core/ConstGlobals.h"
#include "RenderBindlessManager.h"
#include "RHIHandleTypes.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Texture;
class Viewport;
class Buffer;
class Shader;
class RenderPipelineState;
class RenderPass;
class FrameBuffer;
class ComputePipelineState;
class CommandList;
MYRENDERER_BEGIN_CLASS(RenderFactory)
public:
	Int render_api_version=0;
	Bool enable_render_debug=false;
	//  threading_mode replaces enable_rhi_thread
	EThreadingMode threading_mode = EThreadingMode::ThreeThread;
	//  UE  r.Vulkan.EnableValidation — validation levels
	// 0=off, 1=errors only, 2=warnings+errors
	Int validation_level = 0;
	// If true, register debug utils messenger callback (on top of validation layers)
	Bool enable_debug_callback = false;
	// If true, do not crash when validation layers are unavailable (Android behaviour)
	Bool validation_optional = false;
MYRENDERER_END_CLASS

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RenderRHI,public RenderResource)

public:
	RenderRHI() MYDEFAULT;
	VIRTUAL~ RenderRHI() MYDEFAULT;

#pragma region INIT_MATHOD
	VIRTUAL void METHOD(Init)(RenderFactory* render_factory) PURE;

	VIRTUAL void METHOD(PostInit)() PURE;

	VIRTUAL void METHOD(Shutdown)() PURE;

#pragma endregion


#pragma region CREATE_RESOURCE
	VIRTUAL Viewport* METHOD(CreateViewport)(void* window_handle, Int width, Int height, Bool is_full_screen) PURE;
	VIRTUAL Shader* METHOD(CreateShader)(CONST ShaderDesc& desc, CONST ShaderDataPayload& data) PURE;
	VIRTUAL Buffer* METHOD(CreateBuffer)(CONST BufferDesc& buffer_desc) PURE;
	VIRTUAL Texture* METHOD(CreateTexture)(CONST TextureDesc& texture_desc) PURE;
	VIRTUAL RenderPipelineState* METHOD(CreateRenderPipelineState)(CONST RenderGraphiPipelineStateDesc& desc) PURE;
	// --  
	VIRTUAL ComputePipelineState* METHOD(CreateComputePipelineState)(CONST ComputePipelineStateDesc& desc) { return nullptr; }
	VIRTUAL RenderPass* METHOD(CreateRenderPass)(CONST RenderPassDesc& desc) PURE;
	VIRTUAL FrameBuffer* METHOD(CreateFrameBuffer)(CONST FrameBufferDesc& desc) PURE;

	VIRTUAL void* METHOD(MapBuffer)(Buffer* buffer, ENUM_MAP_TYPE map_type, ENUM_MAP_FLAG map_flag) PURE;
	VIRTUAL void METHOD(UnmapBuffer)(Buffer* buffer) PURE;

#pragma endregion

#pragma region DRAW
	VIRTUAL	CommandList* METHOD(GetImmediateCommandList)() PURE;
	// --  
	VIRTUAL CommandList* METHOD(GetCommandListForQueue)(ENUM_QUEUE_TYPE queue_type) { return nullptr; }
	VIRTUAL void METHOD(SubmitCommandList)(CommandList* command_list) PURE;
	// --  
	VIRTUAL void METHOD(SubmitCommandListForQueue)(CommandList* cmd_list, ENUM_QUEUE_TYPE queue_type) {}

	VIRTUAL void METHOD(RenderEnd)() PURE;
	// --   Async RHI thread (lock-free)
	VIRTUAL CommandList* METHOD(GetWriteCommandList)() PURE;
	VIRTUAL CommandList* METHOD(GetRHICmdListForPresent)() PURE;
	VIRTUAL void METHOD(SwapCommandLists)() PURE;
	VIRTUAL Bool METHOD(IsReplayDone)() CONST PURE;
	// Frame-number replay sync: the render thread waits until the RHI thread
	// finished replaying ITS frame (a plain bool can signal the previous
	// frame -> present submits never-replayed commands -> silent drops).
	VIRTUAL UInt64 METHOD(GetSwapFrame)() CONST { return 0; }
	VIRTUAL UInt64 METHOD(GetReplayFrame)() CONST { return 0; }
	VIRTUAL void METHOD(StartRHIThread)() PURE;
	VIRTUAL void METHOD(StopRHIThread)() PURE;
	VIRTUAL BindlessManager* METHOD(GetBindlessManager)() PURE;
#pragma endregion

private:

protected:


MYRENDERER_END_CLASS



MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
extern CORE_API MXRender::RHI::RenderRHI* g_render_rhi;
extern CORE_API Bool g_enable_rhi_thread;

extern CORE_API void METHOD(RHIInit)();
extern CORE_API void METHOD(RHIShutdown)();

extern CORE_API MXRender::RHI::Viewport* METHOD(RHICreateViewport)(void* window_handle, Int width, Int height, Bool is_full_screen);
extern CORE_API MXRender::RHI::TextureHandle METHOD(RHICreateTexture)(CONST MXRender::RHI::TextureDesc& texture_desc, CONST String& name = "");
extern CORE_API MXRender::RHI::BufferHandle METHOD(RHICreateBuffer)(CONST MXRender::RHI::BufferDesc& buffer_desc, CONST String& name = "");
extern CORE_API MXRender::RHI::Shader* METHOD(RHICreateShader)(CONST MXRender::RHI::ShaderDesc& desc, CONST MXRender::RHI::ShaderDataPayload& data);
extern CORE_API MXRender::RHI::PSOHandle METHOD(RHICreateRenderPipelineState)(CONST MXRender::RHI::RenderGraphiPipelineStateDesc& desc, CONST String& name = "");
extern CORE_API MXRender::RHI::RenderPass* METHOD(RHICreateRenderPass)(CONST MXRender::RHI::RenderPassDesc& desc);
extern CORE_API MXRender::RHI::FrameBuffer* METHOD(RHICreateFrameBuffer)(CONST MXRender::RHI::FrameBufferDesc& desc);
extern CORE_API MXRender::RHI::CommandList* METHOD(RHIGetImmediateCommandList)();
// --  
extern CORE_API MXRender::RHI::ComputePipelineState* METHOD(RHICreateComputePipelineState)(CONST MXRender::RHI::ComputePipelineStateDesc& desc);
extern CORE_API MXRender::RHI::CommandList* METHOD(RHIGetCommandListForQueue)(MXRender::ENUM_QUEUE_TYPE queue_type);
extern CORE_API void METHOD(RHISubmitCommandListForQueue)(MXRender::RHI::CommandList* cmd_list, MXRender::ENUM_QUEUE_TYPE queue_type);
extern CORE_API void METHOD(RHIRenderEnd)();
extern CORE_API void METHOD(RHISubmitCommandList)(MXRender::RHI::CommandList* command_list);
// --   Async RHI thread (lock-free)
extern CORE_API MXRender::RHI::CommandList* METHOD(RHIGetWriteCommandList)();
extern CORE_API MXRender::RHI::CommandList* METHOD(RHIGetRHICmdListForPresent)();
extern CORE_API void METHOD(RHISwapCommandLists)();
extern CORE_API Bool METHOD(RHIIsReplayDone)();
extern CORE_API void METHOD(RHIStartRHIThread)();
extern CORE_API void METHOD(RHIStopRHIThread)();
extern CORE_API void* METHOD(RHIMapBuffer)(MXRender::RHI::Buffer* buffer, MXRender::ENUM_MAP_TYPE map_type, MXRender::ENUM_MAP_FLAG map_flag);
extern CORE_API void METHOD(RHIUnmapBuffer)(MXRender::RHI::Buffer* buffer);

extern CORE_API MXRender::RHI::BindlessManager* METHOD(RHIGetBindlessManager)();

#endif

