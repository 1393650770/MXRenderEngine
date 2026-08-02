#pragma once
#ifndef _GAMEPLAY_IPARALLELSYSTEM_
#define _GAMEPLAY_IPARALLELSYSTEM_

// IParallelSystem: a gameplay system that declares its component access and
// runs as a partitioned (index-sliced) task group inside the per-tick
// TaskGraph.
//
// Contract (enforced by registration-time conflict detection + the
// JobContext debug guards):
//   - GetAccess() must declare EVERY component the parallel body writes.
//     write ∩ (read ∪ write) of another parallel system serializes this
//     system (registration order decides who stays parallel).
//   - touches_world_state = true serializes against ALL parallel systems
//     (conservative escape hatch for world-shared state like edit queues).
//   - RunParallel may run on any JobSystem worker: no ECS structural
//     changes (create/destroy/emplace/remove), no RHI calls, no
//     FrameSynchronizer logic APIs inside the body.
//   - PreParallel/PostParallel run sequentially around the group (structural
//     changes ARE allowed there).
//   - The order of parallel systems relative to sequential systems is
//     preserved: the parallel group replaces the registered position, and
//     every sequential system runs after the whole group (dependents).

#include "Core/ConstDefine.h"
#include "Gameplay/ISystem.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Gameplay)

// Resource-level access declaration (component type ids via ComponentTypeID).
struct SystemAccess
{
	Vector<UInt32> read_components;
	Vector<UInt32> write_components;
	Bool touches_world_state = false;   // serializes against all parallel systems
};

class IParallelSystem : public ISystem
{
public:
	VIRTUAL ~IParallelSystem() MYDEFAULT;

	// Declared access; consumed at registration for conflict detection.
	VIRTUAL SystemAccess METHOD(GetAccess)() CONST { return {}; }

	// Sequential hooks around the parallel group (optional).
	VIRTUAL void METHOD(PreParallel)(World::GameWorld& world) { (void)world; }
	VIRTUAL void METHOD(PostParallel)(World::GameWorld& world) { (void)world; }

	// Partitioned body: called once per partition. The default delegates to
	// Run() so a system can be parallel without rewriting its body; systems
	// that partition their ECS iteration override this.
	VIRTUAL void METHOD(RunParallel)(World::GameWorld& world, Float32 dt,
		UInt32 partition_index, UInt32 partition_count)
	{
		(void)partition_index;
		(void)partition_count;
		Run(world, dt);
	}
};

MYRENDERER_END_NAMESPACE  // Gameplay
MYRENDERER_END_NAMESPACE  // MXRender
#endif // _GAMEPLAY_IPARALLELSYSTEM_
