#include "Gameplay/Systems.h"
#include "World/GameWorld.h"
#include "World/PixelCollision.h"
#include "World/TerrainEditCommand.h"
#include "ECS/ECSManager.h"
#include <algorithm>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Gameplay)

SystemAccess MovementSystem::GetAccess() CONST
{
	SystemAccess access;
	access.write_components = { ECS::ComponentTypeID::Get<TransformComp>(), ECS::ComponentTypeID::Get<VelocityComp>() };
	return access;
}

void MovementSystem::Run(World::GameWorld& world, Float32 dt)
{
	// Sequential fallback (0-worker degrade / single-partition path).
	RunParallel(world, dt, 0, 1);
}

void MovementSystem::RunParallel(World::GameWorld& world, Float32 dt,
	UInt32 partition_index, UInt32 partition_count)
{
	auto& ecs = world.GetECS();
	auto& collision = world.GetCollision();
	const Float32 kTick = 1.0f / 60.0f;

	// Parallel partition over the primary Transform storage: index-sliced,
	// zero materialization. Component VALUE writes only - no structural
	// changes (EnTT registry is not thread-safe; JobContext guards this).
	ecs.ParallelForEach<TransformComp, VelocityComp>(
		partition_index, partition_count,
		[&](TransformComp& tf, VelocityComp& vel)
		{
			// Player input is applied by the sample via PlayerComp.speed;
			// here we just integrate + resolve.
			//
			// ResolveAABB's vel parameter is the PER-TICK DISPLACEMENT, not the
			// velocity (it moves pos += vel internally). Pre-integrating
			// (pos + vel*dt) AND passing the full velocity double-moved, and a
			// collision restored only to the pre-integrated position - the
			// player sank 0.005/tick through the stone floor (Bug1).
			glm::vec2 new_pos = tf.pos;                    // current position
			glm::vec2 move = vel.vel * dt;                 // per-tick displacement
			// AABB resolution against solid mask (players only pass through
			// non-solid: sand/water are passable in Phase 3). ResolveAABB is
			// const and lock-free - safe for concurrent partition reads.
			collision.ResolveAABB(new_pos, move, tf.half_size);

			// A zeroed displacement that was non-zero = blocked on that axis ->
			// zero the velocity too (standing on stone keeps vel.y = 0).
			glm::vec2 new_vel = vel.vel;
			if (vel.vel.x != 0.0f && move.x == 0.0f)
				new_vel.x = 0.0f;
			if (vel.vel.y != 0.0f && move.y == 0.0f)
				new_vel.y = 0.0f;

			// Apply gravity (all dynamic entities; velocity units).
			new_vel.y -= 18.0f * kTick;
			if (new_vel.y < -12.0f)
				new_vel.y = -12.0f;

			tf.pos = new_pos;
			vel.vel = new_vel;
		});
}

void ProjectileSystem::Run(World::GameWorld& world, Float32 dt)
{
	(void)dt;
	auto& ecs = world.GetECS();
	auto& collision = world.GetCollision();
	auto& edit_queue = world.GetEditQueue();
	auto& event_bus = world.GetEventBus();

	Vector<MXRender::ECS::EntityHandle> to_destroy;
	ecs.ForEach<TransformComp, VelocityComp, ProjectileComp>(
		[&](MXRender::ECS::EntityHandle handle)
		{
			auto* tf = ecs.GetComponent<TransformComp>(handle);
			auto* vel = ecs.GetComponent<VelocityComp>(handle);
			auto* proj = ecs.GetComponent<ProjectileComp>(handle);
			if (!tf || !vel || !proj)
				return;

			if (proj->lifetime_ticks == 0)
			{
				to_destroy.push_back(handle);
				return;
			}
			--proj->lifetime_ticks;

			// Move along velocity.
			glm::vec2 next = tf->pos + vel->vel * (1.0f / 60.0f);

			// Raycast the movement segment against solid terrain.
			World::PixelCollision::RayHit hit = collision.Raycast(tf->pos, vel->vel, glm::length(vel->vel) / 60.0f + 0.5f);
			if (hit.hit)
			{
				// Explode on impact.
				World::TerrainEditCommand cmd;
				cmd.type = World::TerrainEditType::Explode;
				cmd.x = hit.cell_x;
				cmd.y = hit.cell_y;
				cmd.radius = proj->explosion_radius;
				edit_queue.Push(cmd);

				Gameplay::ExplosionEvent evt;
				evt.pos = glm::vec2((Float32)hit.cell_x, (Float32)hit.cell_y);
				evt.radius = proj->explosion_radius;
				evt.damage = proj->damage;
				event_bus.Publish(evt);

				to_destroy.push_back(handle);
				return;
			}

			tf->pos = next;
		});

	for (MXRender::ECS::EntityHandle h : to_destroy)
		ecs.DestroyEntity(h);
	ecs.GarbageCollect();
}

void CameraFollowSystem::Run(World::GameWorld& world, Float32 dt)
{
	(void)dt;
	// The sample owns the Camera2D; the system is a hook point. Follow logic
	// lives in the sample to avoid a Render dependency in World.
}

MYRENDERER_END_NAMESPACE  // Gameplay
MYRENDERER_END_NAMESPACE  // MXRender