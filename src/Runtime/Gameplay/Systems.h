#pragma once
#ifndef _GAMEPLAY_SYSTEMS_
#define _GAMEPLAY_SYSTEMS_

#include "Core/ConstDefine.h"
#include "Gameplay/ISystem.h"
#include "Gameplay/Components.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Gameplay)

// Integrates Transform + Velocity, resolves against the world solid mask.
// Player entities get WASD input from the sample via the shared input state.
class MovementSystem : public ISystem
{
public:
	VIRTUAL void METHOD(Run)(World::GameWorld& world, Float32 dt) OVERRIDE;
};

// Decrements projectile lifetime, raycasts against solid terrain, and on hit
// pushes an Explode TerrainEditCommand + publishes an ExplosionEvent.
class ProjectileSystem : public ISystem
{
public:
	VIRTUAL void METHOD(Run)(World::GameWorld& world, Float32 dt) OVERRIDE;
};

// Moves the game camera toward the player position (simple follow).
class CameraFollowSystem : public ISystem
{
public:
	VIRTUAL void METHOD(Run)(World::GameWorld& world, Float32 dt) OVERRIDE;
};

// Event payloads
struct ExplosionEvent
{
	glm::vec2 pos{ 0.0f, 0.0f };
	Float32 radius = 0.0f;
	Float32 damage = 0.0f;
};

struct DamageEvent
{
	UInt32 entity_index = 0;   // resolved via handle (see run)
	Float32 amount = 0.0f;
};

MYRENDERER_END_NAMESPACE  // Gameplay
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _GAMEPLAY_SYSTEMS_