#pragma once
#ifndef _GAMEPLAY_COMPONENTS_
#define _GAMEPLAY_COMPONENTS_

#include "Core/ConstDefine.h"
#include <glm/glm.hpp>

// Gameplay ECS components. All are aggregates with default member
// initializers - EnTT emplace<T> requires default-constructibility.
// Each component must be registered once via ECSManager::RegisterComponent<T>
// before any AddComponent call (GameWorld ctor does this).

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Gameplay)

struct TransformComp
{
	glm::vec2 pos{ 0.0f, 0.0f };
	glm::vec2 half_size{ 0.5f, 0.5f };
};

struct VelocityComp
{
	glm::vec2 vel{ 0.0f, 0.0f };
};

struct PlayerComp
{
	Float32 speed = 3.0f;      // world units per second
	Float32 jump_speed = 6.0f;
};

struct HealthComp
{
	Float32 hp = 100.0f;
	Float32 max_hp = 100.0f;
	Bool dead = false;
};

struct ProjectileComp
{
	Float32 damage = 25.0f;
	UInt32 lifetime_ticks = 120;   // 2 seconds at 60 ticks/s
	Float32 explosion_radius = 6.0f;
};

struct WandComp
{
	Float32 cooldown_ticks = 15;   // ticks between shots
	Float32 cooldown_left = 0.0f;
};

MYRENDERER_END_NAMESPACE  // Gameplay
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _GAMEPLAY_COMPONENTS_