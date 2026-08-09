// EnntECSSystem must be fully defined before ECSManager.h is included so the
// RegisterComponent<T> template's dynamic_cast sees a complete type.
#include "ECS/Ennt/EnntECSSystem.h"
#include "World/GameWorld.h"
#include "World/GpuPixelWorld.h"
#include "World/MaterialRegistry.h"
#include "Gameplay/Components.h"
#include "ECS/ECSManager.h"
#include "Audio/AudioManager.h"
#include "Render/LineRenderer/LineRendererManager.h"
#include "Core/Job/JobSystem.h"
#include "Core/ConstGlobals.h"
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

namespace
{
	// Payload for the per-tick task graph steps; lives on the logic thread's
	// stack for the duration of Tick() (Execute blocks until completion).
	struct TickArgs
	{
		GameWorld* world = nullptr;
		Float32 dt = 0.0f;
	};
}

GameWorld::GameWorld(UInt32 seed)
	: edit_queue_(std::make_unique<TerrainEditQueue>())
	, event_bus_(std::make_unique<GameplayEventBus>())
	, collision_(std::make_unique<PixelCollision>())
	, systems_(std::make_unique<Gameplay::SystemRegistry>())
{
	sim_state_.seed = seed;
	sim_state_.rng.Seed(seed);

	// ECS singleton lifecycle owned by the world (Phase 3 accepts the
	// single-instance constraint; a test-world factory is Phase 4 work).
	ECS::ECSManager::Create(new MXRender::ECS::Ennt::EnntECSSystem());
	RegisterComponents();

	simulator_ = CreateSimulator(seed);
	CHECK_WITH_LOG(simulator_ == nullptr, "GameWorld: simulator factory returned null")
	edit_sink_ = dynamic_cast<ITerrainEditSink*>(simulator_);
	CHECK_WITH_LOG(edit_sink_ == nullptr, "GameWorld: simulator is not an ITerrainEditSink")

	// Composition root owns the JobSystem lifecycle (facade singleton).
	Core::JobSystem::Initialize();
}

GameWorld::~GameWorld()
{
	event_bus_->ClearAll();
	systems_->Clear();
	// simulator_ is allocated by the CreateSimulator factory (new GpuPixelWorld)
	// - owned here, must be released before the ECS singleton dies.
	delete simulator_;
	simulator_ = nullptr;
	Core::JobSystem::Shutdown();
	ECS::ECSManager::Destroy();
}

IPixelWorldSimulator* GameWorld::CreateSimulator(UInt32 seed)
{
	// Default backend: GPU (Phase 2). Subclasses override for CPU.
	auto* gpu = new GpuPixelWorld();
	gpu->Init();
	gpu->Reset(seed);
	return gpu;
}

void GameWorld::RegisterComponents()
{
	auto& ecs = GetECS();
	ecs.RegisterComponent<Gameplay::TransformComp>();
	ecs.RegisterComponent<Gameplay::VelocityComp>();
	ecs.RegisterComponent<Gameplay::PlayerComp>();
	ecs.RegisterComponent<Gameplay::HealthComp>();
	ecs.RegisterComponent<Gameplay::ProjectileComp>();
	ecs.RegisterComponent<Gameplay::WandComp>();
}

void GameWorld::SpawnPlayer()
{
	auto& ecs = GetECS();
	MXRender::ECS::EntityHandle player = ecs.CreateEntity();
	ecs.AddComponent<Gameplay::TransformComp>(player);
	ecs.AddComponent<Gameplay::VelocityComp>(player);
	ecs.AddComponent<Gameplay::PlayerComp>(player);
	ecs.AddComponent<Gameplay::HealthComp>(player);
	ecs.AddComponent<Gameplay::WandComp>(player);
	// The sample reads the player handle via a dedicated system query.
}

void GameWorld::Tick()
{
	Float32 dt = 1.0f / 60.0f;

	// Per-tick task graph. RunAllParallel expands the ECS systems into the
	// graph: parallel systems run as partitioned task groups (concurrently on
	// JobSystem workers), sequential systems run after the whole group. The
	// graph is rebuilt every tick - TaskGraph::Reset() reuses its node pool,
	// so the hot path stays allocation-free.
	TickArgs args{ this, dt };
	task_graph_.Reset();
	Core::JobTask* t_systems = systems_->RunAllParallel(*this, dt, task_graph_);
	// Audio and edits both depend on the ECS group (they read its output) and
	// run concurrently with each other.
	Core::JobTask* t_audio = task_graph_.AddTask("Tick.Audio", &TickRunAudio, &args);
	Core::JobTask* t_edits = task_graph_.AddTask("Tick.Edits", &TickFlushEdits, &args);
	Core::JobTask* t_state = task_graph_.AddTask("Tick.State", &TickAdvanceState, &args);
	task_graph_.AddDependency(t_audio, t_systems);
	task_graph_.AddDependency(t_edits, t_systems);
	task_graph_.AddDependency(t_state, t_edits);
	task_graph_.AddDependency(t_state, t_audio);
	// Trail collection last: after state (which depends on all systems +
	// GarbageCollect inside the sequential tail) - the collected set is the
	// tick's surviving entities.
	Core::JobTask* t_trails = task_graph_.AddTask("Tick.Trails", &TickSyncTrails, &args);
	task_graph_.AddDependency(t_trails, t_state);
	task_graph_.Execute(Core::JobSystem::Get());

	// Rotate the snapshot slot at tick end: the app fills the new slot in
	// OnGameTick (right after Tick returns), and the same slot is handed to
	// the render thread via FrameContext.snapshot this frame.
	AdvanceSnapshot();
}

void GameWorld::TickRunAudio(void* arg)
{
	TickArgs* a = static_cast<TickArgs*>(arg);
	// No backend created (most samples) -> no-op. Runs on a JobSystem worker;
	// miniaudio's engine update is thread-safe for a single caller thread.
	if (Audio::AudioManager::IsCreated())
		Audio::AudioManager::Get().Update(a->dt);
}

void GameWorld::TickSyncTrails(void* arg)
{
	TickArgs* a = static_cast<TickArgs*>(arg);
	// No-op when no trail facade was created (samples without trails) -
	// Get() would dereference a null singleton.
	if (!Render::LineRendererManager::IsCreated())
		return;
	Render::LineRendererManager& mgr = Render::LineRendererManager::Get();
	const UInt64 frame = g_frame_number_render_thread.load();
	Render::LineFrameState* out = mgr.GetWriteStates(frame);
	UInt32 count = 0;
	// Reuse the typed iteration path (single partition = sequential collect).
	// Value-copy, never component pointers: components die with entities
	// (GarbageCollect) - pointers would dangle in the render thread.
	a->world->GetECS().ParallelForEach<Render::LineRendererComponent>(
		0, 1,
		[&](Render::LineRendererComponent& c)
		{
			if (count < mgr.GetWriteCapacity())
				out[count++] = Render::MakeLineFrameState(c);
		});
	mgr.SetWriteCount(frame, count);
}

void GameWorld::TickFlushEdits(void* arg)
{
	TickArgs* a = static_cast<TickArgs*>(arg);
	// 2. Flush terrain edits into the simulator + CPU solid mask mirror.
	if (a->world->edit_queue_->HasPending())
	{
		Vector<EditEvent> flushed;
		a->world->edit_queue_->FlushTo(*a->world->edit_sink_, &flushed);
		a->world->SyncSolidMask(flushed);
	}
}

void GameWorld::TickAdvanceState(void* arg)
{
	TickArgs* a = static_cast<TickArgs*>(arg);
	// 3. Advance simulation state (RNG stays on the sequential tail so the
	// consumption order is deterministic regardless of task scheduling).
	++a->world->sim_state_.tick;
	a->world->sim_state_.rng.Next();
}

void GameWorld::Reset(UInt32 seed)
{
	// Full reset contract (see plan Phase 3):
	//   tick/seed/rng -> edit queue -> edit log -> simulator -> ECS ->
	//   collision -> event bus (tokens stay valid) -> respawn player.
	sim_state_.tick = 0;
	sim_state_.seed = seed;
	sim_state_.rng.Seed(seed);
	sim_state_.edit_log.clear();
	sim_state_.next_entity_id = 1;

	edit_queue_->Reset();

	simulator_->Reset(seed);

	ECS::ECSManager::Destroy();
	ECS::ECSManager::Create(new MXRender::ECS::Ennt::EnntECSSystem());
	RegisterComponents();

	collision_->Clear();
	SpawnPlayer();
}

void GameWorld::SyncSolidMask(CONST Vector<EditEvent>& edits)
{
	// Edit-driven CPU solid mask (Phase 3): every edit applied through the
	// queue mirrors into the mask, so player AABB collision and projectile
	// raycasts work against the GPU-authoritative world without a readback.
	// Write overwrites -> mask = IsSolid(mat); Deposit fills only empty
	// cells -> non-solid deposits never change the mask.
	for (CONST auto& edit : edits)
	{
		if (edit.op == kEditOpDeposit && !MaterialRegistry::IsSolid(edit.material))
			continue;
		collision_->SetSolid(edit.x, edit.y, MaterialRegistry::IsSolid(edit.material));
	}
}

ECS::ECSManager& GameWorld::GetECS() CONST
{
	return ECS::ECSManager::Get();
}

Int GameWorld::DebugVerifyDeterminism()
{
	// Phase 3 dev check: replay edit_log through a fresh CPU PixelWorld and
	// compare solid mask divergence. Returns first divergent tick or -1.
	// Full implementation lives with the sample-side determinism harness.
	return -1;
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender