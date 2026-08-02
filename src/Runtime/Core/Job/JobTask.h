#pragma once
#ifndef _JOB_TASK_
#define _JOB_TASK_

// JobTask: a single node in a TaskGraph.
//
// Plain function pointer + void* arg instead of std::function - no per-task
// heap allocation (matters at ~100k-entity workloads). ref_count counts
// unsatisfied dependencies; a task becomes ready when it reaches zero.
// dependents is populated at build time and read-only during execution, so
// no locking is needed on the release path beyond the atomic ref_count.

#include "Core/ConstDefine.h"
#include <atomic>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Core)

struct JobTask
{
	void (*fn)(void*) = nullptr;
	void* arg = nullptr;
	// Called once after fn (may delete arg if the task owns it). The JobSystem
	// invokes it unconditionally - ownership transfer is explicit and safe.
	void (*arg_dtor)(void*) = nullptr;
	const char* name = "JobTask";              // profiling label (stringified by callers)
	std::atomic<UInt32> ref_count{ 0 };        // unsatisfied dependencies; 0 = ready
	Vector<JobTask*> dependents;               // read-only after build
	UInt32 task_id = 0;

	// std::atomic is neither copyable nor movable, so the TaskGraph node pool
	// (Vector<JobTask>) needs explicit move operations.
	JobTask() = default;
	JobTask(JobTask&& other) noexcept
		: fn(other.fn)
		, arg(other.arg)
		, arg_dtor(other.arg_dtor)
		, name(other.name)
		, ref_count(other.ref_count.load(std::memory_order_relaxed))
		, dependents(std::move(other.dependents))
		, task_id(other.task_id)
	{
	}
	JobTask& operator=(JobTask&& other) noexcept
	{
		if (this != &other)
		{
			fn = other.fn;
			arg = other.arg;
			arg_dtor = other.arg_dtor;
			name = other.name;
			ref_count.store(other.ref_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
			dependents = std::move(other.dependents);
			task_id = other.task_id;
		}
		return *this;
	}
	JobTask(const JobTask&) = delete;
	JobTask& operator=(const JobTask&) = delete;

	// Return to a pristine state for pool reuse.
	void Reset()
	{
		fn = nullptr;
		arg = nullptr;
		arg_dtor = nullptr;
		name = "JobTask";
		ref_count.store(0, std::memory_order_relaxed);
		dependents.clear();
		task_id = 0;
	}
};

MYRENDERER_END_NAMESPACE  // Core
MYRENDERER_END_NAMESPACE  // MXRender
#endif // _JOB_TASK_
