#if PLATFORM_WGPU

#include "Platform/Emscripten/EmscriptenWindow.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderViewport.h"
#include "RHI/WebGPU/WGPU_RenderRHI.h"
#include "Render/RenderInterface.h"
#include "Render/Core/RenderFrameData.h"
#include <emscripten.h>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)

EmscriptenWindow::EmscriptenWindow(const String& title, UInt32 w, UInt32 h)
	: m_width((Int)w), m_height((Int)h)
{
	// Configure the canvas element to match the requested size
	emscripten_set_canvas_element_size(m_canvas_selector.c_str(), (int)w, (int)h);
	std::cout << "[WGPU] EmscriptenWindow created: " << w << "x" << h << std::endl;
}

EmscriptenWindow::~EmscriptenWindow()
{
	if (m_viewport) { delete m_viewport; m_viewport = nullptr; }
}

void EmscriptenWindow::GetFramebufferSize(Int& w, Int& h) CONST
{
	int cw = 0, ch = 0;
	emscripten_get_canvas_element_size(m_canvas_selector.c_str(), &cw, &ch);
	w = (Int)cw;
	h = (Int)ch;
}

Float64 EmscriptenWindow::GetTime() CONST
{
	// emscripten_get_now returns milliseconds as double
	return emscripten_get_now() / 1000.0;
}

void EmscriptenWindow::StartEventLoop()
{
	if (!m_render)
	{
		std::cout << "[WGPU] StartEventLoop: null render interface" << std::endl;
		return;
	}

	m_last_frame_time = (float)GetTime();

	// Register the frame callback. simulate_infinite_loop=1 means this call
	// never returns — the browser drives the loop via requestAnimationFrame.
	std::cout << "[WGPU] StartEventLoop: registering main loop" << std::endl;
	emscripten_set_main_loop_arg(FrameCallback, this, 0, 1);
}

void EmscriptenWindow::InitRHIAndViewport()
{
	if (m_init_done || !m_render) return;

	// RHIInit was already called in Window::InitWindow; the async device request
	// has now completed (IsReady()==true). Create the viewport (surface + swapchain).
	m_viewport = RHICreateViewport(GetNativeHandle(), m_width, m_height, false);
	CHECK_WITH_LOG(m_viewport == nullptr, "WGPU: RHICreateViewport returned null");

	// Single-threaded path: call OnInit (wraps OnInit_Logic + OnInit_Render)
	m_render->OnInit(this, m_viewport);
	m_init_done = true;
	std::cout << "[WGPU] InitRHIAndViewport done" << std::endl;
}

void EmscriptenWindow::ShutdownRHI()
{
	if (m_render) m_render->OnShutdown();
	if (m_viewport) { delete m_viewport; m_viewport = nullptr; }
	RHIShutdown();
}

void EmscriptenWindow::FrameCallback(void* arg)
{
	auto* self = STATIC_CAST(arg, EmscriptenWindow);
	if (!self || !self->m_render) return;

	// Poll async WebGPU init: adapter/device requests are callback-based
	auto* wgpu_rhi = static_cast<RHI::WebGPU::WGPU_RenderRHI*>(g_render_rhi);
	if (!wgpu_rhi || !wgpu_rhi->IsReady())
	{
		// Device not ready yet — browser will call us again next frame
		return;
	}

	// First frame after ready: create viewport and call OnInit
	if (!self->m_init_done)
	{
		self->InitRHIAndViewport();
		if (!self->m_init_done) return;  // init failed
	}

	// ---- Per-frame lifecycle (Single-threaded mode) ----
	float current_frame = (float)self->GetTime();
	float delta_time = current_frame - self->m_last_frame_time;
	self->m_last_frame_time = current_frame;
	// Clamp delta_time: first frame / tab-refocus spikes would break sim
	if (delta_time < 0.0f || delta_time > 0.25f) delta_time = 1.0f / 60.0f;

	self->PollEvents();

	self->m_render->OnUpdate(delta_time);

	Render::FrameContext ctx;
	ctx.deltaTime = delta_time;

	// Query current canvas size and detect resize (GetFramebufferSize takes Int&)
	Int fb_w = 0, fb_h = 0;
	self->GetFramebufferSize(fb_w, fb_h);
	if (fb_w <= 0 || fb_h <= 0)
	{
		fb_w = self->m_width;
		fb_h = self->m_height;
	}
	ctx.viewport_width = (UInt32)fb_w;
	ctx.viewport_height = (UInt32)fb_h;

	// Resize swapchain if canvas dimensions changed (browser window resize)
	if (self->m_viewport && (fb_w != self->m_width || fb_h != self->m_height))
	{
		std::cout << "[WGPU] Resize: " << self->m_width << "x" << self->m_height
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

// WebGPU platform factory — platform_data is nullptr on browser (canvas is implicit)
UniquePtr<PlatformWindow> CreatePlatformWindow(const String& title, UInt32 w, UInt32 h, void* platform_data)
{
	return std::make_unique<EmscriptenWindow>(title, w, h);
}

MYRENDERER_END_NAMESPACE

#endif // PLATFORM_WGPU
