#pragma once
#ifndef _PROFILING_
#define _PROFILING_

// Profiling macro facade. When MYRENDERER_ENABLE_PROFILING is defined the
// macros map to Optick events; otherwise they compile to nothing (zero
// overhead, no header dependency - wasm builds never define the flag).
//
// Usage:
//   PROFILE_SCOPE("GameWorld::Tick")
//   PROFILE_FRAME()
//   PROFILE_TAG("chunks", count)
//
// IMPORTANT: PROFILE_SCOPE names must be compile-time string literals
// (OPTICK_EVENT requirement); use PROFILE_SCOPE_DYNAMIC for runtime names.

#if defined(MYRENDERER_ENABLE_PROFILING)
#include <optick.h>
#define PROFILE_SCOPE(name)          OPTICK_EVENT(name)
#define PROFILE_SCOPE_DYNAMIC(name)  OPTICK_EVENT_DYNAMIC(name)
#define PROFILE_FRAME()              OPTICK_FRAME("MainThread")
#define PROFILE_TAG(name, value)     OPTICK_TAG(name, value)
#else
#define PROFILE_SCOPE(name)
#define PROFILE_SCOPE_DYNAMIC(name)
#define PROFILE_FRAME()
#define PROFILE_TAG(name, value)
#endif

#endif // _PROFILING_