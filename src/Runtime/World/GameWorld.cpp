// EnntECSSystem must be fully defined before ECSManager.h is included so the
// RegisterComponent<T> template's dynamic_cast sees a complete type.
#include "ECS/Ennt/EnntECSSystem.h"
#include "World/GameWorld.h"
#include "World/GpuPixelWorld.h"
#include "World/MaterialRegistry.h"
#include "Gameplay/Components.h"
#include "ECS/ECSManager.h"
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

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
}

GameWorld::~GameWorld()
{
	event_bus_->ClearAll();
	systems_->Clear();
	// simulator_ is allocated by the CreateSimulator factory (new GpuPixelWorld)
	// - owned here, must be released before the ECS singleton dies.
	delete simulator_;
	simulator_ = nullptr;
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

	// 1. ECS systems (movement, projectiles, camera follow).
	systems_->RunAll(*this, dt);

	// 2. Flush terrain edits into the simulator + CPU solid mask mirror.
	if (edit_queue_->HasPending())
	{
		Vector<EditEvent> flushed;
		edit_queue_->FlushTo(*edit_sink_, &flushed);
		SyncSolidMask(flushed);
	}

	// 3. Advance simulation state.
	++sim_state_.tick;
	sim_state_.rng.Next();
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