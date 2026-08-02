#pragma once

#ifndef _CONSTGLOBALS_
#define _CONSTGLOBALS_

#include "ConstDefine.h"
#include <atomic>

// Atomic: written by the logic thread (Window), read by the render thread
// (RHIRenderEnd -> deferred-release age checks). The lockstep happens-before
// chain disappears once the frame sync switches to lead-by-1.
extern CORE_API std::atomic<UInt64> g_frame_number_render_thread;
extern CORE_API CONST UInt64 g_max_frame_number;

//  线程模式配置
enum class EThreadingMode : UInt8
{
	Single,       // 单线程：Logic + Render + RHI 全部同线程（默认）
	RHIThread,    // 双线程：Logic+Render 主线程 + RHI 线程
	ThreeThread,  // 三线程：Logic + Render + RHI 各自独立线程
};
extern CORE_API EThreadingMode g_thread_mode;

#endif 
