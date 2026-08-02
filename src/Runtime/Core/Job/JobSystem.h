#pragma once
#ifndef _JOB_SYSTEM_
#define _JOB_SYSTEM_

// JobSystem: worker pool + DAG dispatcher (facade singleton, UIManager
// pattern; lifecycle owned by the GameWorld composition root).
//
// Initialize(0) auto-picks max(1, hardware_concurrency/2 - 2): the hwc/2 is a
// physical-core estimate on hyper-threaded CPUs (logical-core counts would
// oversubscribe 8C16T to 14 workers), and -2 reserves cores for the logic and
// render/RHI threads. A computed worker count of 0 degrades to fully
// sequential execution - the Android/Web fallback path (never spawns).
//
// Scheduling: single global FIFO ready queue (a per-worker LIFO + random
// stealing is a Phase-3 scaling refinement; the mutex contention is
// negligible at Phase-1 partition granularity).
//
// Deadlock model: worker threads NEVER block on graph completion (no WaitAll
// inside task bodies - every worker spinning on its own child tasks would
// starve). Execute() runs on the logic thread, which participates in task
// execution until the graph is complete.

#include "Core/ConstDefine.h"
#include "Core/Job/JobTask.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Core)

class TaskGraph;

// Per-thread execution context. Parallel task bodies set the flag so the
// debug-verification layer (e.g. ECS structural-change guards) can detect
// illegal operations inside a parallel window. All inline, zero overhead.
namespace JobContext
{
	inline thread_local Bool s_in_parallel_task = false;
	inline void EnterParallelTask() { s_in_parallel_task = true; }
	inline void ExitParallelTask() { s_in_parallel_task = false; }
	inline Bool IsInParallelTask() { return s_in_parallel_task; }
}

class JobSystem
{
public:
	// ---- Facade ----
	static JobSystem& Get();                        // function-local static instance
	static void Initialize(UInt32 worker_count = 0); // 0 = auto (see header); idempotent
	static void Shutdown();                          // joins workers; idempotent

	// ---- Dispatch ----
	// Blocking; the calling (logic) thread participates until completion.
	void ExecuteGraph(TaskGraph& graph);

	// ---- Query ----
	UInt32 GetWorkerCount() const { return (UInt32)workers_.size(); }
	Bool IsMultiThreaded() const { return !workers_.empty(); }
	Bool IsRunning() const { return running_.load(std::memory_order_acquire); }

private:
	JobSystem() = default;
	~JobSystem() = default;
	JobSystem(const JobSystem&) = delete;
	JobSystem& operator=(const JobSystem&) = delete;

	void WorkerMain(UInt32 worker_index);
	void RunReadyTask(JobTask* task);               // exec fn + release dependents
	void PushReady(JobTask* task);
	JobTask* PopReady();                            // global FIFO

	std::atomic<Bool> running_{ false };
	std::atomic<UInt32> remaining_tasks_{ 0 };

	Vector<std::thread> workers_;

	std::mutex ready_mtx_;
	std::deque<JobTask*> ready_queue_;

	std::mutex done_mtx_;
	std::condition_variable done_cv_;               // notified when remaining_tasks_ hits 0
};

MYRENDERER_END_NAMESPACE  // Core
MYRENDERER_END_NAMESPACE  // MXRender
#endif // _JOB_SYSTEM_
