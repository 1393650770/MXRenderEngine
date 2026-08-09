#include "Core/Job/JobSystem.h"
#include "Core/Job/TaskGraph.h"
#include "Core/Job/JobProfiling.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Core)

JobSystem& JobSystem::Get()
{
	static JobSystem s_instance;
	return s_instance;
}

void JobSystem::Initialize(UInt32 worker_count)
{
	JobSystem& js = Get();
	if (js.IsRunning())
		return;  // idempotent

	if (worker_count == 0)
	{
		// Explicit override via env var: MX_JOB_WORKERS=N (0 = fully
		// sequential degrade - the Android/Web path, also useful for
		// scaling sweeps and race bisection).
		const char* env = std::getenv("MX_JOB_WORKERS");
		if (env && env[0])
		{
			worker_count = (UInt32)std::atoi(env);
		}
		else
		{
			// Physical-core estimate (hwc/2 on hyper-threaded CPUs) minus 2
			// cores reserved for the logic thread and render/RHI threads.
			// Zero or one spare cores degrade to sequential execution.
			// NOTE: parenthesized (std::max) - windows.h min/max macros break
			// the unparenthesized form (see CLAUDE.md).
			UInt32 hwc = (UInt32)std::thread::hardware_concurrency();
			UInt32 physical = (std::max)(1u, hwc / 2);
			worker_count = (physical > 2) ? (physical - 2) : 0;
		}
	}

	js.running_.store(true, std::memory_order_release);
	for (UInt32 i = 0; i < worker_count; ++i)
		js.workers_.emplace_back(&JobSystem::WorkerMain, &js, i);
}

void JobSystem::Shutdown()
{
	JobSystem& js = Get();
	if (!js.IsRunning())
		return;  // idempotent

	js.running_.store(false, std::memory_order_release);
	{
		std::lock_guard<std::mutex> lock(js.done_mtx_);
		js.done_cv_.notify_all();
	}
	for (auto& t : js.workers_)
	{
		if (t.joinable())
			t.join();
	}
	js.workers_.clear();
	{
		std::lock_guard<std::mutex> lock(js.ready_mtx_);
		js.ready_queue_.clear();
	}
}

void JobSystem::ExecuteGraph(TaskGraph& graph)
{
	if (graph.IsEmpty())
		return;

	remaining_tasks_.store(graph.GetTaskCount(), std::memory_order_release);
	{
		std::lock_guard<std::mutex> lock(ready_mtx_);
		for (UInt32 i = 0; i < graph.GetTaskCount(); ++i)
		{
			JobTask* t = graph.GetTask(i);
			if (t->ref_count.load(std::memory_order_acquire) == 0)
				ready_queue_.push_back(t);
		}
	}

	// The logic thread participates as an executor until the graph completes.
	// It may block on done_cv_ (it is not a worker, so waiting cannot starve
	// the workers still draining the queue).
	while (remaining_tasks_.load(std::memory_order_acquire) != 0)
	{
		JobTask* task = PopReady();
		if (task)
		{
			RunReadyTask(task);
			continue;
		}
		std::unique_lock<std::mutex> lock(done_mtx_);
		done_cv_.wait_for(lock, std::chrono::milliseconds(2),
			[this] { return remaining_tasks_.load(std::memory_order_acquire) == 0; });
	}
}

void JobSystem::RunReadyTask(JobTask* task)
{
	PROFILE_JOB_SCOPE(task->name)

	if (task->fn)
		task->fn(task->arg);
	if (task->arg_dtor)
		task->arg_dtor(task->arg);

	// Release dependents; the last unmet dependency of a task pushes it ready.
	for (JobTask* dependent : task->dependents)
	{
		if (dependent->ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1)
			PushReady(dependent);
	}

	// Last completed task wakes everyone waiting in ExecuteGraph.
	if (remaining_tasks_.fetch_sub(1, std::memory_order_acq_rel) == 1)
	{
		std::lock_guard<std::mutex> lock(done_mtx_);
		done_cv_.notify_all();
	}
}

void JobSystem::PushReady(JobTask* task)
{
	{
		std::lock_guard<std::mutex> lock(ready_mtx_);
		ready_queue_.push_back(task);
	}
	// Workers wake on their short idle timeout; a dedicated per-worker
	// condition variable is a Phase-3 refinement.
}

JobTask* JobSystem::PopReady()
{
	std::lock_guard<std::mutex> lock(ready_mtx_);
	if (ready_queue_.empty())
		return nullptr;
	JobTask* task = ready_queue_.front();
	ready_queue_.pop_front();
	return task;
}

void JobSystem::WorkerMain(UInt32 worker_index)
{
	(void)worker_index;
	PROFILE_SET_THREAD_NAME("MXJobWorker")

	while (running_.load(std::memory_order_acquire))
	{
		JobTask* task = PopReady();
		if (task)
		{
			RunReadyTask(task);
			continue;
		}
		// Idle: park briefly. PushReady relies on this short timeout to wake
		// workers; waking is not latency-critical for per-tick graphs.
		std::unique_lock<std::mutex> lock(done_mtx_);
		done_cv_.wait_for(lock, std::chrono::milliseconds(1));
	}
}

MYRENDERER_END_NAMESPACE  // Core
MYRENDERER_END_NAMESPACE  // MXRender
