#include "Gameplay/SystemRegistry.h"
#include "Core/Job/JobSystem.h"
#include "World/GameWorld.h"
#include "ECS/ECSManager.h"
#include <algorithm>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Gameplay)

namespace
{
	bool ContainsComp(const Vector<UInt32>& comps, UInt32 type_id)
	{
		return std::find(comps.begin(), comps.end(), type_id) != comps.end();
	}

	// Per-partition payload; heap-allocated per partition task, deleted by the
	// task body (ownership is explicit in the AddTask arg_dtor contract).
	struct PartitionArgs
	{
		World::GameWorld* world = nullptr;
		Float32 dt = 0.0f;
		UInt32 partition_index = 0;
		UInt32 partition_count = 1;
		IParallelSystem* system = nullptr;
	};

	struct HookArgs
	{
		World::GameWorld* world = nullptr;
		IParallelSystem* system = nullptr;
	};

	// Sequential-tail payload (must live at namespace scope: referenced by the
	// SystemRegistry::RunSequentialTail static member).
	struct TailArgs
	{
		World::GameWorld* world = nullptr;
		Float32 dt = 0.0f;
		SystemRegistry* self = nullptr;
		Vector<UInt32> indices;
	};
}

void SystemRegistry::Register(String debug_name, UniquePtr<ISystem> system)
{
	if (!system)
		return;

	Bool parallel = false;
	if (auto* p = dynamic_cast<IParallelSystem*>(system.get()))
	{
		SystemAccess access = p->GetAccess();
		if (!access.touches_world_state)
		{
			// Conflict detection (BIDIRECTIONAL): this system's write set must
			// not intersect another parallel system's (read ∪ write), AND this
			// system's read set must not intersect another's write set (a
			// read-vs-write race is as real as write-vs-write - e.g. TrailUpdate
			// reads Transform while Movement writes it). First registration
			// wins; later systems are demoted to sequential.
			parallel = true;
			for (UInt32 i = 0; i < systems_.size(); ++i)
			{
				if (!parallel_flags_[i])
					continue;
				const SystemAccess& other = accesses_[i];
				for (UInt32 w : access.write_components)
				{
					if (ContainsComp(other.read_components, w) || ContainsComp(other.write_components, w))
					{
						parallel = false;
						break;
					}
				}
				if (parallel)
				{
					for (UInt32 r : access.read_components)
					{
						if (ContainsComp(other.write_components, r))
						{
							parallel = false;
							break;
						}
					}
				}
				if (!parallel)
					break;
			}
			if (parallel)
			{
				accesses_.push_back(access);
			}
		}
	}
	if (!parallel)
		accesses_.push_back({});

	parallel_flags_.push_back(parallel);
	systems_.push_back({ std::move(debug_name), std::move(system) });
}

void SystemRegistry::RunAll(World::GameWorld& world, Float32 dt)
{
	for (auto& entry : systems_)
	{
		if (entry.second)
			entry.second->Run(world, dt);
	}
}

Core::JobTask* SystemRegistry::RunAllParallel(World::GameWorld& world, Float32 dt, Core::TaskGraph& graph)
{
	// Group: parallel systems (flags) vs sequential systems (registration
	// order preserved). Sequential tail depends on every parallel group.
	Vector<IParallelSystem*> parallel_systems;
	Vector<UInt32> sequential_indices;
	for (UInt32 i = 0; i < systems_.size(); ++i)
	{
		if (!systems_[i].second)
			continue;
		if (parallel_flags_[i])
		{
			if (auto* p = dynamic_cast<IParallelSystem*>(systems_[i].second.get()))
				parallel_systems.push_back(p);
			else
				sequential_indices.push_back(i);
		}
		else
		{
			sequential_indices.push_back(i);
		}
	}

	UInt32 worker_count = Core::JobSystem::Get().GetWorkerCount();
	UInt32 partition_count = (std::max)(1u, worker_count * 2);

	Vector<Core::JobTask*> group_completions;  // one per parallel system (PostParallel)

	for (IParallelSystem* sys : parallel_systems)
	{
		// PreParallel (sequential hook).
		Core::JobTask* pre = graph.AddTask("PreParallel",
			[](void* arg) {
				auto* a = static_cast<HookArgs*>(arg);
				a->system->PreParallel(*a->world);
				delete a;
			},
			new HookArgs{ &world, sys });

		// Partitioned bodies: index-sliced, marked as parallel tasks so the
		// debug-verification layer (ECS structural-change guards) can detect
		// illegal calls inside the window.
		Vector<Core::JobTask*> partitions;
		partitions.reserve(partition_count);
		for (UInt32 pi = 0; pi < partition_count; ++pi)
		{
			Core::JobTask* t = graph.AddTask("RunParallel",
				[](void* arg) {
					auto* a = static_cast<PartitionArgs*>(arg);
					Core::JobContext::EnterParallelTask();
					a->system->RunParallel(*a->world, a->dt, a->partition_index, a->partition_count);
					Core::JobContext::ExitParallelTask();
					delete a;
				},
				new PartitionArgs{ &world, dt, pi, partition_count, sys });
			graph.AddDependency(t, pre);
			partitions.push_back(t);
		}

		// PostParallel (sequential hook; joins this system's partitions).
		Core::JobTask* post = graph.AddTask("PostParallel",
			[](void* arg) {
				auto* a = static_cast<HookArgs*>(arg);
				a->system->PostParallel(*a->world);
				delete a;
			},
			new HookArgs{ &world, sys });
		for (Core::JobTask* t : partitions)
			graph.AddDependency(post, t);
		group_completions.push_back(post);
	}

	// Sequential tail: all non-parallel systems, registration order, one
	// task. Depends on every parallel group completion.
	Core::JobTask* tail = graph.AddTask("SequentialTail", &SystemRegistry::RunSequentialTail,
		new TailArgs{ &world, dt, this, std::move(sequential_indices) });
	for (Core::JobTask* c : group_completions)
		graph.AddDependency(tail, c);
	return tail;
}

void SystemRegistry::RunSequentialTail(void* arg)
{
	auto* a = static_cast<TailArgs*>(arg);
	for (UInt32 idx : a->indices)
	{
		if (idx < a->self->systems_.size() && a->self->systems_[idx].second)
			a->self->systems_[idx].second->Run(*a->world, a->dt);
	}
	delete a;
}

void SystemRegistry::Clear()
{
	systems_.clear();
	accesses_.clear();
	parallel_flags_.clear();
}

MYRENDERER_END_NAMESPACE  // Gameplay
MYRENDERER_END_NAMESPACE  // MXRender
