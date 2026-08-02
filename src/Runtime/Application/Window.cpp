#include "Window.h"
#include "Application/SampleApp.h"
#if PLATFORM_ANDROID
#include "Platform/Android/AndroidWindow.h"
#endif
#if PLATFORM_WGPU
#include "Platform/Emscripten/EmscriptenWGPUWindow.h"
#endif
#if PLATFORM_GLES3
#include "Platform/Emscripten/EmscriptenGLWindow.h"
#endif

#include <iostream>
#include <memory>
#include "RHI/RenderRHI.h"
#include "RHI/RenderViewport.h"
#include "Render/RenderInterface.h"
#include "RHI/RenderCommandList.h"
#include "Render/Core/CommandQueue.h"
#include "Network/NetworkManager.h"
#include <limits>
#include <thread>
#include <cstdlib>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

Window::Window(CONST String& in_title, UInt32 in_w, UInt32 in_h, void* platform_data)
	: title(in_title), width(in_w), height(in_h)
{
	if (!platform_data)
		platform_data = SampleApp::GetPlatformData();
	platform_window = CreatePlatformWindow(title, width, height, platform_data);
}

void Window::InitWindow()
{
	if (platform_window->IsMobile())
		return; // Android: RHI init deferred to when window becomes ready
	RHIInit();
#if PLATFORM_WGPU || PLATFORM_GLES3
	// WebGPU/GLES3: viewport creation deferred to platform window's InitRHIAndViewport
#if PLATFORM_WGPU
	// (WGPU: async device init must complete before CreateViewport can succeed).
#else
	// (GLES3: GL context created in EmscriptenGLWindow, viewport follows).
#endif
#else
	viewport = RHICreateViewport(platform_window->GetNativeHandle(), width, height, is_full_screen);
#endif
}

void Window::Run(RenderInterface* render)
{
#if PLATFORM_ANDROID
	if (auto* aw = dynamic_cast<AndroidWindow*>(platform_window.get()))
	{
		aw->SetRenderInterface(render);
		aw->StartEventLoop();
		return;
	}
#endif
#if PLATFORM_WGPU || PLATFORM_GLES3
#if PLATFORM_WGPU
	if (auto* ew = dynamic_cast<EmscriptenWindow*>(platform_window.get()))
	{
		ew->SetRenderInterface(render);
		ew->StartEventLoop();
		return;
	}
#else
	if (auto* ew = dynamic_cast<EmscriptenGLWindow*>(platform_window.get()))
	{
		ew->SetRenderInterface(render);
		ew->StartEventLoop();
		return;
	}
#endif
#endif

	EThreadingMode mode = g_thread_mode;
	Bool use_rhi_thread = (mode >= EThreadingMode::RHIThread);
	Bool use_render_thread = (mode >= EThreadingMode::ThreeThread);

	// Command queue: without a dedicated render thread (Single/RHIThread) the
	// queue bypasses and executes enqueued commands inline on the calling
	// thread - the UE "!ShouldExecuteOnRenderThread()" fallback.
	Render::GetRenderCommandQueue().SetBypass(!use_render_thread);

	// Frame-sync debug switch: MX_FORCE_LOCKSTEP=1 keeps the logic thread
	// blocking on WaitFrameComplete (pre-lead behavior) - use it to bisect
	// whether a bug is caused by the lead-by-N frame overlap.
	{
		const char* env = std::getenv("MX_FORCE_LOCKSTEP");
		frame_sync.SetForceLockstep(env && env[0] == '1' && env[0] != '0');
	}

	if (use_rhi_thread) {
		RHIStartRHIThread();
	}
	if (use_render_thread) {
		render->OnInit_Logic(platform_window.get(), viewport);
		frame_sync.StartRenderThread(render, viewport);
	} else {
		render->OnInit(platform_window.get(), viewport);
	}

	Int fw = 0, fh = 0;

	while (!platform_window->ShouldClose())
	{
		float currentFrame = static_cast<float>(platform_window->GetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		platform_window->PollEvents();

		render->OnUpdate(deltaTime);
			MXRender::Network::NetworkManager::Get().Update();

		// Framebuffer size read ONCE per frame, before the mode switch: the
		// ThreeThread branch forwards changes via FrameContext.needs_resize
		// (the render thread owns Resize there); Single/RHIThread resize
		// directly below.
		platform_window->GetFramebufferSize(fw, fh);
		while (fw == 0 || fh == 0)
		{
			platform_window->GetFramebufferSize(fw, fh);
			// PlatformWindow does not have WaitEvents — the loop itself is the wait
		}

		switch (mode)
		{
		case EThreadingMode::Single:
		{
			Render::FrameContext ctx;
			ctx.deltaTime = deltaTime;
			ctx.viewport_width = this->width;
			ctx.viewport_height = this->height;
			render->OnPrepareFrameContext(ctx);
			render->OnPreRender(ctx);
			render->OnRender();
			render->OnPostRender(ctx);
			auto* cmd_list = RHIGetImmediateCommandList();
			viewport->Present(cmd_list, true, true);
			break;
		}

		case EThreadingMode::RHIThread:
		{
			Render::FrameContext ctx;
			ctx.deltaTime = deltaTime;
			ctx.viewport_width = this->width;
			ctx.viewport_height = this->height;
			render->OnPrepareFrameContext(ctx);
			render->OnPreRender(ctx);
			auto* cmd_list = RHIGetWriteCommandList();
			cmd_list->SetBypass(false);
			cmd_list->Begin();
			render->OnRender();
			render->OnPostRender(ctx);
			cmd_list->SetBypass(true);
			RHISwapCommandLists();

			while (!RHIIsReplayDone()) {
				std::this_thread::yield();
			}
			auto* present_cb = RHIGetRHICmdListForPresent();
			viewport->Present(present_cb, true, true);
			break;
		}

		case EThreadingMode::ThreeThread:
		{
			// Logic-leads-render: recycle the oldest completed frame without
			// blocking (the render thread is a frame behind by design). The
			// lockstep debug path (MX_FORCE_LOCKSTEP=1) keeps the old
			// blocking behavior for race bisection.
			if (!frame_sync.IsForceLockstep())
				frame_sync.TryRecycleCompleted();

			Render::FrameContext* ctx = frame_sync.AcquireWriteSlot();
			if (ctx)
			{
				ctx->deltaTime = deltaTime;
				ctx->viewport_width = this->width;
				ctx->viewport_height = this->height;
				ctx->frame_number = g_frame_number_render_thread.load();
				// Resize ownership moved to the render thread: forward size
				// changes through the frame context (single Resize caller).
				ctx->needs_resize = ((UInt32)fw != this->width) || ((UInt32)fh != this->height);
				ctx->resize_width = fw;
				ctx->resize_height = fh;
				render->OnPrepareFrameContext(*ctx);
			}

			frame_sync.SignalFrameReady();
			if (frame_sync.IsForceLockstep())
				frame_sync.WaitFrameComplete();

			// Frame-sync telemetry (every 300 frames): in-flight count should
			// wander 1-3 in lead mode (0-1 under lockstep). Back-pressure
			// tuning knob for Phase 3.
			static UInt32 s_telemetry = 0;
			if (++s_telemetry % 300 == 0)
				std::cout << "[FrameSync] in_flight=" << frame_sync.GetFramesInFlight()
					<< " lead=" << (frame_sync.IsForceLockstep() ? "lockstep" : "lead") << std::endl;
			break;
		}
		}

		if (mode != EThreadingMode::ThreeThread)
		{
			RHIRenderEnd();
		}
		g_frame_number_render_thread.store((g_frame_number_render_thread.load() + 1) % g_max_frame_number);

		this->width = (UInt32)fw;
		this->height = (UInt32)fh;
		// ThreeThread: the render thread owns Resize (via needs_resize).
		// Single/RHIThread: resize directly on this (main) thread.
		if (mode != EThreadingMode::ThreeThread)
			viewport->Resize((UInt32)fw, (UInt32)fh);
	}

	if (use_render_thread) {
		frame_sync.StopRenderThread();
		render->OnShutdown_Logic();
	} else {
		render->OnShutdown();
	}
	if (use_rhi_thread) RHIStopRHIThread();

	delete viewport;
	viewport = nullptr;
	RHIShutdown();
}

PlatformWindow* Window::GetPlatformWindow() CONST
{
	return platform_window.get();
}

MXRender::RHI::Viewport* Window::GetViewport() CONST
{
	return viewport;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
