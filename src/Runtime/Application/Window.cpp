#include "Window.h"
#include "Application/SampleApp.h"
#if PLATFORM_ANDROID
#include "Platform/Android/AndroidWindow.h"
#endif
#if PLATFORM_WGPU
#include "Platform/WGPU/EmscriptenWindow.h"
#endif
#if PLATFORM_GLES3
#include "Platform/GLES3/EmscriptenGLWindow.h"
#endif

#include <iostream>
#include <memory>
#include "RHI/RenderRHI.h"
#include "RHI/RenderViewport.h"
#include "Render/RenderInterface.h"
#include "RHI/RenderCommandList.h"
#include "Render/Core/CommandQueue.h"
#include <limits>
#include <thread>

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
			Render::FrameContext* ctx = frame_sync.AcquireWriteSlot();
			if (ctx)
			{
				ctx->deltaTime = deltaTime;
				ctx->viewport_width = this->width;
				ctx->viewport_height = this->height;
				ctx->frame_number = g_frame_number_render_thread;
				render->OnPrepareFrameContext(*ctx);
			}

			frame_sync.SignalFrameReady();
			frame_sync.WaitFrameComplete();
			break;
		}
		}

		if (mode != EThreadingMode::ThreeThread)
		{
			RHIRenderEnd();
		}
		g_frame_number_render_thread = (g_frame_number_render_thread + 1) % g_max_frame_number;

		platform_window->GetFramebufferSize(fw, fh);
		while (fw == 0 || fh == 0)
		{
			platform_window->GetFramebufferSize(fw, fh);
			// PlatformWindow does not have WaitEvents — the loop itself is the wait
		}
		this->width = (UInt32)fw;
		this->height = (UInt32)fh;
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
