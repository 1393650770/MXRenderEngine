#if PLATFORM_GLES3

#include "Platform/Emscripten/EmscriptenGLWindow.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderViewport.h"
#include "RHI/GLES3/GLES3_RenderRHI.h"
#include "Render/RenderInterface.h"
#include "Render/Core/RenderFrameData.h"
#include "Input/InputSystem.h"
#include <emscripten.h>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)

// JS→C++ canvas size bridge: set by mx_set_canvas_size() from game.js preRun,
// read by InitRHIAndViewport() to create the viewport at native device resolution.
static Int g_mx_canvas_w = 0;
static Int g_mx_canvas_h = 0;

EmscriptenGLWindow::EmscriptenGLWindow(const String& title, UInt32 w, UInt32 h)
	: m_width((Int)w), m_height((Int)h)
{
	// Do NOT overwrite canvas size here — mini-game runtimes (Douyin/WeChat)
	// create the canvas at native device resolution via tt/wx.createCanvas().
	// Attempting to detect the size via EM_ASM or emscripten_get_canvas_element_size
	// fails at constructor time (WASM execution context may not see polyfilled DOM).
	// Instead, InitRHIAndViewport() will read the actual framebuffer size
	// and create the viewport with the correct dimensions.
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

	std::cout << "[GLES3] StartEventLoop: registering input callbacks" << std::endl;
	RegisterInputCallbacks();

	std::cout << "[GLES3] StartEventLoop: registering main loop" << std::endl;
	emscripten_set_main_loop_arg(FrameCallback, this, 0, 1);
}

// ---- HTML5 Input Callbacks ----

static EM_BOOL OnKeyDown(int eventType, const EmscriptenKeyboardEvent* e, void* userData)
{
	(void)eventType;
	auto* self = STATIC_CAST(userData, EmscriptenGLWindow);
	(void)userData;
	MXRender::Input::InputSystem::Get().FeedKeyDown(e->keyCode);
	return EM_TRUE;
}

static EM_BOOL OnKeyUp(int eventType, const EmscriptenKeyboardEvent* e, void* userData)
{
	(void)eventType;
	auto* self = STATIC_CAST(userData, EmscriptenGLWindow);
	(void)userData;
	MXRender::Input::InputSystem::Get().FeedKeyUp(e->keyCode);
	return EM_TRUE;
}

static EM_BOOL OnMouseDown(int eventType, const EmscriptenMouseEvent* e, void* userData)
{
	(void)eventType;
	auto* self = STATIC_CAST(userData, EmscriptenGLWindow);
	(void)userData;
	MXRender::Input::InputSystem::Get().FeedMouseButton(e->button, true);
	return EM_TRUE;
}

static EM_BOOL OnMouseUp(int eventType, const EmscriptenMouseEvent* e, void* userData)
{
	(void)eventType;
	auto* self = STATIC_CAST(userData, EmscriptenGLWindow);
	(void)userData;
	MXRender::Input::InputSystem::Get().FeedMouseButton(e->button, false);
	return EM_TRUE;
}

static EM_BOOL OnMouseMove(int eventType, const EmscriptenMouseEvent* e, void* userData)
{
	(void)eventType;
	auto* self = STATIC_CAST(userData, EmscriptenGLWindow);
	(void)userData;
	MXRender::Input::InputSystem::Get().FeedMousePos((Float32)e->canvasX, (Float32)e->canvasY);
	return EM_TRUE;
}

static EM_BOOL OnTouchStart(int eventType, const EmscriptenTouchEvent* e, void* userData)
{
	auto* self = STATIC_CAST(userData, EmscriptenGLWindow);
	TouchState ts;
	for (int i = 0; i < e->numTouches && i < (int)TouchState::kMaxPointers; ++i)
	{
		ts.pointers[i].active = true;
		ts.pointers[i].x = (Float32)e->touches[i].canvasX;
		ts.pointers[i].y = (Float32)e->touches[i].canvasY;
		ts.pointers[i].id = e->touches[i].identifier;
	}
	ts.pointer_count = e->numTouches;
	self->FeedTouchState(ts);
	MXRender::Input::InputSystem::Get().FeedTouch(ts);
	return EM_TRUE;
}

static EM_BOOL OnTouchMove(int eventType, const EmscriptenTouchEvent* e, void* userData)
{
	return OnTouchStart(eventType, e, userData);
}

static EM_BOOL OnTouchEnd(int eventType, const EmscriptenTouchEvent* e, void* userData)
{
	auto* self = STATIC_CAST(userData, EmscriptenGLWindow);
	TouchState ts;
	ts.pointer_count = 0;
	self->FeedTouchState(ts);
	MXRender::Input::InputSystem::Get().FeedTouch(ts);
	return EM_TRUE;
}

static EM_BOOL OnWheel(int eventType, const EmscriptenWheelEvent* e, void* userData)
{
	(void)eventType;
	auto* self = STATIC_CAST(userData, EmscriptenGLWindow);
	(void)userData;
	MXRender::Input::InputSystem::Get().FeedMousePos((Float32)e->mouse.canvasX, (Float32)e->mouse.canvasY);
	MXRender::Input::InputSystem::Get().FeedScroll((Float32)e->deltaY);
	return EM_TRUE;
}

void EmscriptenGLWindow::RegisterInputCallbacks()
{
	const char* target = m_canvas_selector.c_str();
	emscripten_set_keydown_callback(target, this, EM_TRUE, OnKeyDown);
	emscripten_set_keyup_callback(target, this, EM_TRUE, OnKeyUp);
	emscripten_set_mousedown_callback(target, this, EM_TRUE, OnMouseDown);
	emscripten_set_mouseup_callback(target, this, EM_TRUE, OnMouseUp);
	emscripten_set_mousemove_callback(target, this, EM_TRUE, OnMouseMove);
	emscripten_set_touchstart_callback(target, this, EM_TRUE, OnTouchStart);
	emscripten_set_touchmove_callback(target, this, EM_TRUE, OnTouchMove);
	emscripten_set_touchend_callback(target, this, EM_TRUE, OnTouchEnd);
	emscripten_set_wheel_callback(target, this, EM_TRUE, OnWheel);
	emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, EM_TRUE, OnResize);
}

EM_BOOL EmscriptenGLWindow::OnResize(int eventType, const EmscriptenUiEvent* e, void* userData)
{
	(void)eventType; (void)e;
	auto* self = STATIC_CAST(userData, EmscriptenGLWindow);
	Int w, h;
	self->GetFramebufferSize(w, h);
	if (self->m_viewport && w > 0 && h > 0)
	{
		self->m_viewport->Resize((UInt32)w, (UInt32)h);
		self->m_width = w;
		self->m_height = h;
	}
	return EM_TRUE;
}

void EmscriptenGLWindow::InitRHIAndViewport()
{
	if (m_init_done || !m_render) return;

	// Use canvas size from JS if available (set via mx_set_canvas_size in
	// game.js preRun). The constructor defaults to 1280x960, but the phone's
	// native resolution (from tt/wx.createCanvas()) is different.
	if (g_mx_canvas_w > 0 && g_mx_canvas_h > 0)
	{
		m_width  = g_mx_canvas_w;
		m_height = g_mx_canvas_h;
	}

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
	MXRender::Input::InputSystem::Get().BeginFrame();
}

// GLES3 platform factory
UniquePtr<PlatformWindow> CreatePlatformWindow(const String& title, UInt32 w, UInt32 h, void* platform_data)
{
	return std::make_unique<EmscriptenGLWindow>(title, w, h);
}

extern "C" {
	EMSCRIPTEN_KEEPALIVE void mx_set_canvas_size(int w, int h) {
		g_mx_canvas_w = w;
		g_mx_canvas_h = h;
		std::cout << "[GLES3] JS canvas size: " << w << "x" << h << std::endl;
	}
}

MYRENDERER_END_NAMESPACE

// JS-to-C++ mouse input bridge (bypasses Emscripten event callbacks)
extern "C" {
	EMSCRIPTEN_KEEPALIVE void mx_feed_mouse_move(float x, float y) {
		MXRender::Input::InputSystem::Get().FeedMousePos(x, y);
	}
	EMSCRIPTEN_KEEPALIVE void mx_feed_mouse_down(int btn) {
		MXRender::Input::InputSystem::Get().FeedMouseButton(btn, true);
	}
	EMSCRIPTEN_KEEPALIVE void mx_feed_mouse_up(int btn) {
		MXRender::Input::InputSystem::Get().FeedMouseButton(btn, false);
	}
	EMSCRIPTEN_KEEPALIVE void mx_feed_scroll(float delta) {
		MXRender::Input::InputSystem::Get().FeedScroll(delta);
	}
}
#endif // PLATFORM_GLES3
