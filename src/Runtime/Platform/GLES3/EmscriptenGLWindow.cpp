#if PLATFORM_GLES3

#include "Platform/GLES3/EmscriptenGLWindow.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderViewport.h"
#include "RHI/GLES3/GLES3_RenderRHI.h"
#include "Render/RenderInterface.h"
#include "Render/Core/RenderFrameData.h"
#include <emscripten.h>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)

EmscriptenGLWindow::EmscriptenGLWindow(const String& title, UInt32 w, UInt32 h)
	: m_width((Int)w), m_height((Int)h)
{
	emscripten_set_canvas_element_size(m_canvas_selector.c_str(), (int)w, (int)h);
	std::cout << "[GLES3] EmscriptenGLWindow created: " << w << "x" << h << std::endl;
}

EmscriptenGLWindow::~EmscriptenGLWindow()
{
	if (m_viewport) { delete m_viewport; m_viewport = nullptr; }
}

void EmscriptenGLWindow::GetFramebufferSize(Int& w, Int& h) CONST
{
	int cw = 0, ch = 0;
	emscripten_get_canvas_element_size(m_canvas_selector.c_str(), &cw, &ch);
	w = (Int)cw;
	h = (Int)ch;
}

Float64 EmscriptenGLWindow::GetTime() CONST
{
	return emscripten_get_now() / 1000.0;
}

void EmscriptenGLWindow::StartEventLoop()
{
	if (!m_render)
	{
		std::cout << "[GLES3] StartEventLoop: null render interface" << std::endl;
		return;
	}

	m_last_frame_time = (float)GetTime();

	std::cout << "[GLES3] StartEventLoop: registering main loop" << std::endl;
	// simulate_infinite_loop=1: this call never returns
	emscripten_set_main_loop_arg(FrameCallback, this, 0, 1);
}

void EmscriptenGLWindow::InitRHIAndViewport()
{
	if (m_init_done || !m_render) return;

	// RHIInit was already called in Window::InitWindow.
	// Create the viewport (which triggers emscripten_webgl_create_context
	// inside GLES3_RenderRHI::CreateViewport).
	m_viewport = RHICreateViewport(GetNativeHandle(), m_width, m_height, false);
	CHECK_WITH_LOG(m_viewport == nullptr, "GLES3: RHICreateViewport returned null");

	m_render->OnInit(this, m_viewport);
	m_init_done = true;
	std::cout << "[GLES3] InitRHIAndViewport done" << std::endl;
}

void EmscriptenGLWindow::ShutdownRHI()
{
	if (m_render) m_render->OnShutdown();
	if (m_viewport) { delete m_viewport; m_viewport = nullptr; }
	RHIShutdown();
}

void EmscriptenGLWindow::FrameCallback(void* arg)
{
	auto* self = STATIC_CAST(arg, EmscriptenGLWindow);
	if (!self || !self->m_render) return;

	// GLES3 Init is synchronous — no async polling needed (unlike WebGPU).
	// Check that RHI and GL context are ready.
	if (!g_render_rhi) return;

	auto* gles_rhi = static_cast<RHI::GLES3::GLES3_RenderRHI*>(g_render_rhi);
	if (!gles_rhi->IsReady())
	{
		return;
	}

	// First frame after ready: create viewport and call OnInit
	if (!self->m_init_done)
	{
		self->InitRHIAndViewport();
		if (!self->m_init_done) return;
	}

	// ---- Per-frame lifecycle (Single-threaded mode) ----
	float current_frame = (float)self->GetTime();
	float delta_time = current_frame - self->m_last_frame_time;
	self->m_last_frame_time = current_frame;
	if (delta_time < 0.0f || delta_time > 0.25f) delta_time = 1.0f / 60.0f;

	self->PollEvents();

	self->m_render->OnUpdate(delta_time);

	Render::FrameContext ctx;
	ctx.deltaTime = delta_time;

	Int fb_w = 0, fb_h = 0;
	self->GetFramebufferSize(fb_w, fb_h);
	if (fb_w <= 0 || fb_h <= 0) { fb_w = self->m_width; fb_h = self->m_height; }
	ctx.viewport_width = (UInt32)fb_w;
	ctx.viewport_height = (UInt32)fb_h;

	if (self->m_viewport && (fb_w != self->m_width || fb_h != self->m_height))
	{
		std::cout << "[GLES3] Resize: " << self->m_width << "x" << self->m_height
		          << " -> " << fb_w << "x" << fb_h << std::endl;
		self->m_width = fb_w;
		self->m_height = fb_h;
		self->m_viewport->Resize((UInt32)fb_w, (UInt32)fb_h);
	}

	self->m_render->OnPrepareFrameContext(ctx);
	self->m_render->OnPreRender(ctx);
	self->m_render->OnRender();
	self->m_render->OnPostRender(ctx);

	auto* cmd_list = RHIGetImmediateCommandList();
	if (self->m_viewport && cmd_list)
	{
		self->m_viewport->Present(cmd_list, true, true);
	}

	RHIRenderEnd();
}

// GLES3 platform factory
UniquePtr<PlatformWindow> CreatePlatformWindow(const String& title, UInt32 w, UInt32 h, void* platform_data)
{
	return std::make_unique<EmscriptenGLWindow>(title, w, h);
}

MYRENDERER_END_NAMESPACE

#endif // PLATFORM_GLES3
