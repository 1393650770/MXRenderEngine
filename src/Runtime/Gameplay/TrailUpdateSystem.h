#pragma once
#ifndef _GAMEPLAY_TRAILUPDATESYSTEM_
#define _GAMEPLAY_TRAILUPDATESYSTEM_

// TrailUpdateSystem: appends the entity position into its
// LineRendererComponent history every tick (engine-automatic trail update -
// register in SystemRegistry and it just works, zero sample boilerplate).
//
// Registration-order dependency: reads TransformComp, writes
// LineRendererComponent -> conflicts with Movement (writes Transform) at
// registration time -> auto-demoted to sequential, runs after Projectile
// (projectile moves first, trail appends after - trail tip = current
// projectile position).

#include "Core/ConstDefine.h"
#include "Gameplay/IParallelSystem.h"
#include "Gameplay/Components.h"
#include "Render/LineRenderer/LineRendererComponent.h"
#include "ECS/ComponentTypeID.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Gameplay)

class TrailUpdateSystem : public IParallelSystem
{
public:
	VIRTUAL SystemAccess METHOD(GetAccess)() CONST OVERRIDE
	{
		SystemAccess access;
		access.read_components = { ECS::ComponentTypeID::Get<TransformComp>() };
		access.write_components = { ECS::ComponentTypeID::Get<Render::LineRendererComponent>() };
		return access;
	}

	VIRTUAL void METHOD(Run)(World::GameWorld& world, Float32 dt) OVERRIDE
	{
		RunParallel(world, dt, 0, 1);   // sequential fallback = single partition
	}

	VIRTUAL void METHOD(RunParallel)(World::GameWorld& world, Float32 dt,
		UInt32 partition_index, UInt32 partition_count) OVERRIDE
	{
		(void)dt;
		auto& ecs = world.GetECS();
		// Parallel partition (storage direct access): append the current
		// position for every entity with a trail component. Component value
		// writes only (JobContext guard; no structural changes).
		ecs.ParallelForEach<TransformComp, Render::LineRendererComponent>(
			partition_index, partition_count,
			[&](TransformComp& tf, Render::LineRendererComponent& lr)
			{
				if (lr.visible)
					lr.AddPoint(tf.pos);
			});
	}
};

MYRENDERER_END_NAMESPACE  // Gameplay
MYRENDERER_END_NAMESPACE  // MXRender
#endif // _GAMEPLAY_TRAILUPDATESYSTEM_
