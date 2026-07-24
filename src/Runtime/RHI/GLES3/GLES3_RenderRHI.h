#pragma once
#ifndef _GLES3_RENDERRHI_
#define _GLES3_RENDERRHI_

#if PLATFORM_GLES3

#include "RHI/RenderRHI.h"
#include <GLES3/gl3.h>
#include <emscripten/html5.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

class GLES3_Viewport;
class GLES3_CommandBuffer;
class GLES3_PipelineState;

// GLES3_RenderRHI: GLES3 (WebGL 2.0 via Emscripten) backend for the RHI abstraction.
// Synchronous init: emscripten_webgl_create_context is blocking on the browser main thread.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(GLES3_RenderRHI, public RenderRHI)

#pragma region METHOD
public:
	GLES3_RenderRHI() MYDEFAULT;
	VIRTUAL ~GLES3_RenderRHI() MYDEFAULT;

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

	// GL context access
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE METHOD(GetGLContext)() CONST { return m_gl_context; }
	Bool METHOD(IsReady)() CONST { return m_gl_context != 0; }

protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE m_gl_context = 0;

	GLES3_CommandBuffer* m_immediate_cmd = nullptr;
	GLES3_Viewport* m_viewport = nullptr;

	// PSO ownership (Decision #8 / SS 12.12): GLES3_RenderRHI owns all PSOs.
	// External code must NOT delete PSO pointers.
	Vector<GLES3_PipelineState*> m_pso_storage;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _GLES3_RENDERRHI_
