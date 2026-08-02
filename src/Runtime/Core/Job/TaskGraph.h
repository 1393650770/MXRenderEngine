#pragma once
#ifndef _TASK_GRAPH_
#define _TASK_GRAPH_

// TaskGraph: dependency DAG for one logical tick (UE TaskGraph-style, minimal).
//
// Build the same phase structure every tick (input -> ECS group -> physics ->
// audio -> snapshot), then Execute. Reset() keeps the node pool allocated, so
// the per-tick hot path performs zero heap allocations. Validate() (Kahn
// cycle check) should be called once after a structural rebuild, not per tick.
//
// Deadlock rules (enforced by convention, mirrored in CLAUDE.md):
//   - Execute() is called by the logic thread, which PARTICIPATES in the
//     execution until the graph completes. Workers never block on the graph.
//   - No WaitAll inside a task body. Fork/join is expressed through barriers.

#include "Core/ConstDefine.h"
#include "Core/Job/JobTask.h"
#include <deque>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Core)

class JobSystem;

class TaskGraph
{
public:
	TaskGraph() = default;
	~TaskGraph() = default;
	TaskGraph(const TaskGraph&) = delete;
	TaskGraph& operator=(const TaskGraph&) = delete;

	// ---- Build (logic thread, single-threaded) ----
	// arg_dtor (optional) runs after fn; pass it when the task owns heap arg.
	JobTask* AddTask(const char* name, void (*fn)(void*), void* arg, void (*arg_dtor)(void*) = nullptr);
	void AddDependency(JobTask* task, JobTask* depends_on);   // task runs after depends_on
	JobTask* AddBarrier(const Vector<JobTask*>& group);       // fires when the group is done

	// Kahn cycle detection; call once after a structural rebuild.
	Bool Validate();

	// ---- Execute ----
	// Blocking. The calling (logic) thread participates until completion.
	void Execute(JobSystem& js);

	// ---- Pool reuse ----
	void Reset();                              // tasks cleared, node pool kept
	UInt32 GetTaskCount() const { return (UInt32)tasks_.size(); }
	Bool IsEmpty() const { return tasks_.empty(); }
	JobTask* GetTask(UInt32 index) const { return tasks_[index]; }

private:
	// Node pool. std::deque on purpose: AddTask hands out pointers that stay
	// valid until Reset() - a std::vector would invalidate them on realloc
	// (moving JobTask objects changes their addresses mid-build).
	Vector<JobTask*> tasks_;
	std::deque<JobTask> pool_;                 // capacity retained across Reset()
	UInt32 pool_used_ = 0;
};

MYRENDERER_END_NAMESPACE  // Core
MYRENDERER_END_NAMESPACE  // MXRender
#endif // _TASK_GRAPH_
