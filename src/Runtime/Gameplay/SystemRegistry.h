#pragma once
#ifndef _SYSTEM_REGISTRY_
#define _SYSTEM_REGISTRY_

#include "Core/ConstDefine.h"
#include "Core/Job/TaskGraph.h"
#include "Gameplay/ISystem.h"
#include "Gameplay/IParallelSystem.h"
#include <utility>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Gameplay)

// Registry of gameplay systems. Registration order == execution order
// (documented contract). Name is debug metadata only.
//
// Parallel scheduling: IParallelSystem registrations declare component access;
// at registration time a system whose write set intersects another parallel
// system's (read ∪ write) set is demoted to sequential (conservative: later
// registrations lose). RunAllParallel expands the tick graph: parallel
// systems run as partitioned task groups (concurrently), every sequential
// system runs after the whole group in registration order (dependents).
class SystemRegistry
{
public:
	SystemRegistry() MYDEFAULT;
	~SystemRegistry() MYDEFAULT;

	void METHOD(Register)(String debug_name, UniquePtr<ISystem> system);
	void METHOD(RunAll)(World::GameWorld& world, Float32 dt);
	// Expands this tick's graph with the system group and returns the
	// completion task (sequential tail) for dependents to chain onto.
	Core::JobTask* METHOD(RunAllParallel)(World::GameWorld& world, Float32 dt, Core::TaskGraph& graph);
	void METHOD(Clear)();

protected:

private:
	static void RunSequentialTail(void* arg);

	Vector<std::pair<String, UniquePtr<ISystem>>> systems_;
	Vector<SystemAccess> accesses_;      // parallel systems only (aligned, empty for sequential)
	Vector<Bool> parallel_flags_;        // aligned with systems_: true = parallel executor
};

MYRENDERER_END_NAMESPACE  // Gameplay
MYRENDERER_END_NAMESPACE  // MXRender
#endif // _SYSTEM_REGISTRY_
