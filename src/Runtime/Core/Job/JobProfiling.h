#pragma once
#ifndef _JOB_PROFILING_
#define _JOB_PROFILING_

// Job profiling facade. Worker threads register a name once at startup; task
// execution scopes show up as Optick events when MYRENDERER_ENABLE_PROFILING
// is defined, otherwise everything compiles to nothing.

#include "Core/Profiling.h"

// Per-task execution scope (task names are runtime strings).
#define PROFILE_JOB_SCOPE(name) PROFILE_SCOPE_DYNAMIC(name)

// Worker thread name (Optick API; no-op when profiling is disabled).
#if defined(MYRENDERER_ENABLE_PROFILING)
#define PROFILE_SET_THREAD_NAME(name) OPTICK_SET_THREAD_NAME(name)
#else
#define PROFILE_SET_THREAD_NAME(name)
#endif

#endif // _JOB_PROFILING_
