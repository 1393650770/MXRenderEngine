#include "Core/Job/TaskGraph.h"
#include "Core/Job/JobSystem.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Core)

JobTask* TaskGraph::AddTask(const char* name, void (*fn)(void*), void* arg, void (*arg_dtor)(void*))
{
	JobTask* task = nullptr;
	if (pool_used_ < pool_.size())
	{
		task = &pool_[pool_used_];
		task->Reset();
	}
	else
	{
		pool_.emplace_back();
		task = &pool_.back();
	}
	++pool_used_;

	task->fn = fn;
	task->arg = arg;
	task->arg_dtor = arg_dtor;
	task->name = name ? name : "JobTask";
	task->task_id = (UInt32)tasks_.size();
	tasks_.push_back(task);
	return task;
}

void TaskGraph::AddDependency(JobTask* task, JobTask* depends_on)
{
	depends_on->dependents.push_back(task);
	task->ref_count.fetch_add(1, std::memory_order_relaxed);
}

JobTask* TaskGraph::AddBarrier(const Vector<JobTask*>& group)
{
	// A barrier is a no-op task that depends on every task in the group; it
	// becomes ready only when the whole group has completed.
	JobTask* barrier = AddTask("Barrier", nullptr, nullptr);
	for (JobTask* t : group)
		AddDependency(barrier, t);
	return barrier;
}

Bool TaskGraph::Validate()
{
	// Kahn's algorithm on the dependency edges (task -> its dependents).
	Map<JobTask*, UInt32> indegree;
	indegree.reserve(tasks_.size());
	Vector<JobTask*> queue;
	for (JobTask* t : tasks_)
	{
		UInt32 d = t->ref_count.load(std::memory_order_acquire);
		indegree[t] = d;
		if (d == 0)
			queue.push_back(t);
	}

	UInt32 visited = 0;
	while (!queue.empty())
	{
		JobTask* t = queue.back();
		queue.pop_back();
		++visited;
		for (JobTask* dependent : t->dependents)
		{
			UInt32 left = --indegree[dependent];
			if (left == 0)
				queue.push_back(dependent);
		}
	}
	return visited == tasks_.size();
}

void TaskGraph::Reset()
{
	pool_used_ = 0;
	tasks_.clear();
}

void TaskGraph::Execute(JobSystem& js)
{
	js.ExecuteGraph(*this);
}

MYRENDERER_END_NAMESPACE  // Core
MYRENDERER_END_NAMESPACE  // MXRender
