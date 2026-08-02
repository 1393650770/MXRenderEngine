#pragma once
#ifndef _GAME_WORLD_
#define _GAME_WORLD_

#include "Core/ConstDefine.h"
#include "Core/Job/TaskGraph.h"
#include "World/SimulationState.h"
#include "World/GameplayEventBus.h"
#include "World/TerrainEditCommand.h"
#include "World/PixelCollision.h"
#include "World/FrameSnapshot.h"
#include "Gameplay/SystemRegistry.h"
#include <memory>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(ECS)
class ECSManager;
MYRENDERER_END_NAMESPACE
MYRENDERER_BEGIN_NAMESPACE(World)
class IPixelWorldSimulator;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// GameWorld: composition root (Facade + Mediator) owning all gameplay
// subsystems. Subsystems never reference each other directly - they only
// interact through the narrow getters below (zero-direct-reference rule,
// verifiable by grep). GameWorld owns the ECS singleton lifecycle.
MYRENDERER_BEGIN_CLASS(GameWorld)
#pragma region METHOD
public:
	explicit GameWorld(UInt32 seed);
	~GameWorld();

	void METHOD(Tick)();
	void METHOD(Reset)(UInt32 seed);

	// ---- narrow subsystem access ----
	ECS::ECSManager& METHOD(GetECS)() CONST;
	IPixelWorldSimulator* METHOD(GetSimulator)() CONST { return simulator_; }
	TerrainEditQueue& METHOD(GetEditQueue)() CONST { return *edit_queue_; }
	GameplayEventBus& METHOD(GetEventBus)() CONST { return *event_bus_; }
	PixelCollision& METHOD(GetCollision)() CONST { return *collision_; }
	SimulationState& METHOD(GetSimState)() { return sim_state_; }
	Gameplay::SystemRegistry& METHOD(GetSystems)() CONST { return *systems_; }

	// Factory: which simulator backend (CPU/GPU) the world uses.
	VIRTUAL IPixelWorldSimulator* METHOD(CreateSimulator)(UInt32 seed);

	// Edit sink view of the active simulator (CPU applies, GPU enqueues).
	ITerrainEditSink* METHOD(GetEditSink)() CONST { return edit_sink_; }

	// ---- Frame snapshot pool (render-minimum world view) ----
	// The app fills GetWriteSnapshot() at tick end (OnGameTick); the snapshot
	// pointer is handed to the render thread via FrameContext.snapshot
	// (GameApp::OnPrepareFrameContext). Depth matches FrameSynchronizer's
	// in-flight frames so the render thread never reads a reused slot.
	static constexpr UInt32 kSnapshotDepth = 3;
	FrameSnapshot* METHOD(GetWriteSnapshot)() { return &snapshots_[snapshot_slot_]; }
	void METHOD(AdvanceSnapshot)() { snapshot_slot_ = (snapshot_slot_ + 1) % kSnapshotDepth; }

	// Dev-time determinism check: replays the edit log through a CPU world
	// and compares the solid mask for the first divergence tick.
	Int METHOD(DebugVerifyDeterminism)();

protected:
	void METHOD(SpawnPlayer)();
	void METHOD(SyncSolidMask)(CONST Vector<EditEvent>& edits);
	void METHOD(RegisterComponents)();

	// Per-tick task graph steps (sequential tail of the tick). Invoked on
	// JobSystem workers after the ECS parallel group completes.
	static void TickFlushEdits(void* arg);
	static void TickAdvanceState(void* arg);
	// Audio update runs in the parallel band right AFTER the ECS group (it
	// reads player positions etc. that systems wrote). No-op when no audio
	// backend was created.
	static void TickRunAudio(void* arg);

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	SimulationState sim_state_;
	IPixelWorldSimulator* simulator_ = nullptr;      // owned by subclass factory
	ITerrainEditSink* edit_sink_ = nullptr;          // simulator as edit sink
	UniquePtr<TerrainEditQueue> edit_queue_;
	UniquePtr<GameplayEventBus> event_bus_;
	UniquePtr<PixelCollision> collision_;
	UniquePtr<Gameplay::SystemRegistry> systems_;
	Core::TaskGraph task_graph_;   // per-tick phase graph (pool reused)
	FrameSnapshot snapshots_[kSnapshotDepth]{};
	UInt32 snapshot_slot_ = 0;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _GAME_WORLD_