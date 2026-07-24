#pragma once
#ifndef _EMSCRIPTEN_GL_WINDOW_
#define _EMSCRIPTEN_GL_WINDOW_

#if PLATFORM_GLES3

#include "Platform/PlatformWindow.h"
#include <emscripten/html5.h>
#include <functional>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Viewport;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)

class RenderInterface;

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(EmscriptenGLWindow, public PlatformWindow)
#pragma region METHOD
public:
	EmscriptenGLWindow(const String& title, UInt32 w, UInt32 h);
	VIRTUAL ~EmscriptenGLWindow() OVERRIDE;

	// PlatformWindow overrides
	VIRTUAL void* GetNativeHandle() CONST OVERRIDE FINAL { return (void*)m_canvas_selector.c_str(); }
	VIRTUAL void  GetFramebufferSize(Int& w, Int& h) CONST OVERRIDE FINAL;
	VIRTUAL void  PollEvents() OVERRIDE FINAL {}
	VIRTUAL Bool  ShouldClose() CONST OVERRIDE FINAL { return m_destroy_requested; }
	VIRTUAL Float64 GetTime() CONST OVERRIDE FINAL;
	VIRTUAL Bool  IsMobile() CONST OVERRIDE FINAL { return false; }

	void SetRenderInterface(RenderInterface* render) { m_render = render; }

	// Non-blocking: registers emscripten_set_main_loop_arg and returns.
	// The frame callback creates the GL context + viewport on the first frame,
	// then runs the render lifecycle each subsequent frame.
	void StartEventLoop();

protected:
	static void FrameCallback(void* arg);
	void InitRHIAndViewport();
	void ShutdownRHI();
private:
#pragma endregion

#pragma region MEMBER
protected:
	String m_canvas_selector = "#canvas";
	Int    m_width = 0;
	Int    m_height = 0;
	Bool   m_destroy_requested = false;
	Bool   m_init_done = false;

	MXRender::RHI::Viewport* m_viewport = nullptr;
	RenderInterface* m_render = nullptr;

	float m_last_frame_time = 0.0f;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE

#endif // PLATFORM_GLES3
#endif // _EMSCRIPTEN_GL_WINDOW_
