#pragma once
#ifndef _GLES3_VIEWPORT_
#define _GLES3_VIEWPORT_

#if PLATFORM_GLES3

#include "RHI/RenderViewport.h"
#include <GLES3/gl3.h>
#include <emscripten/html5.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

class GLES3_Texture;

// GLES3_Viewport: owns the EGL context + canvas swapchain.
// Created by GLES3_RenderRHI::CreateViewport.
// For GLES3 on Emscripten, the swapchain is the default framebuffer (0) —
// emscripten_set_canvas_element_size controls the canvas, and glClear + present
// are implicit via requestAnimationFrame (browser swaps automatically).
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(GLES3_Viewport, public Viewport)
#pragma region METHOD
public:
	GLES3_Viewport(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx, UInt32 width, UInt32 height);
	VIRTUAL ~GLES3_Viewport() OVERRIDE;

	VIRTUAL Texture* METHOD(GetCurrentBackBufferRTV)() OVERRIDE FINAL;
	VIRTUAL Texture* METHOD(GetCurrentBackBufferDSV)() OVERRIDE FINAL;
	VIRTUAL Vector<UInt32> METHOD(GetViewportSize)() CONST OVERRIDE FINAL;
	VIRTUAL UInt32 METHOD(GetViewportSizeWidth)() CONST OVERRIDE FINAL;
	VIRTUAL UInt32 METHOD(GetViewportSizeHeight)() CONST OVERRIDE FINAL;
	VIRTUAL void METHOD(Resize)(UInt32 in_width, UInt32 in_height) OVERRIDE FINAL;
	VIRTUAL void METHOD(Present)(CommandList* in_cmd_list, bool is_present, bool is_lock_to_vsync) OVERRIDE FINAL;
	VIRTUAL void METHOD(AttachUiLayer)(UI::UIBase* ui_layer) OVERRIDE FINAL {}

	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE METHOD(GetContext)() CONST { return m_context; }
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE m_context = 0;
	UInt32 m_width = 0;
	UInt32 m_height = 0;

	GLES3_Texture* m_backbuffer_tex = nullptr;  // wraps the default framebuffer color
	GLES3_Texture* m_depth_tex = nullptr;       // wraps the default framebuffer depth/stencil
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _GLES3_VIEWPORT_
