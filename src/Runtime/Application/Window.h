#pragma once
#ifndef _WINDOW_
#define _WINDOW_
#include "Core/ConstDefine.h"
#include "Core/ConstGlobals.h"
#include "Platform/PlatformWindow.h"
#include "Render/Core/RenderFrameSync.h"
// TaskScheduler: excluded globally (_aligned_malloc removed in MSVC 2022+)
// #include <MTScheduler.h>

#include <array>
#include <functional>
#include <vector>
#include <memory>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Viewport;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
class RenderInterface;
MYRENDERER_BEGIN_NAMESPACE(Application)

MYRENDERER_BEGIN_CLASS(Window)
#pragma region METHOD
public:
	Window(CONST String& in_title = "MXRender", UInt32 in_w = 1280, UInt32 in_h = 960,
	       void* platform_data = nullptr);
	VIRTUAL ~Window() MYDEFAULT;
	void METHOD(InitWindow)();
	void METHOD(Run)(RenderInterface* render);
	PlatformWindow* METHOD(GetPlatformWindow)() CONST;
	MXRender::RHI::Viewport* METHOD(GetViewport)() CONST;
protected:
private:
#pragma endregion

#pragma region MEMBER
public:
protected:
	UniquePtr<PlatformWindow> platform_window;
	Float32 deltaTime = 0.0f;
	Float32 lastFrame = 0.0f;
	UInt32 width = 1280, height = 960;
	String title = "MXRender";
	Bool is_full_screen = false;
	MXRender::RHI::Viewport* viewport = nullptr;
// TaskScheduler excluded globally
//	MT::TaskScheduler scheduler;
	MXRender::Render::FrameSynchronizer frame_sync;
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif //_WINDOW_
