#pragma once
#ifndef _MATERIAL_DEF_
#define _MATERIAL_DEF_

#include "Core/ConstDefine.h"
#include "World/PixelWorldConstants.h"
#include <glm/glm.hpp>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

enum class MaterialPhase : UInt8
{
	Empty = 0,
	Powder,   // sand-like: falls straight, side-slides
	Liquid,   // water-like: falls, spreads sideways
	Gas,      // smoke/steam: rises
	Solid,    // stone/wood: static, collision mask
	Fire      // flame: burns neighbors, lifetime-limited
};

// Immutable material definition. stable_id is BOTH the persistent identifier
// and the 8-bit slot index in MaterialRegistry (slot 0 = Empty, reserved).
MYRENDERER_BEGIN_STRUCT(MaterialDef)
	UInt8 stable_id = kMaterialEmpty;     // == registry slot index
	String name;
	MaterialPhase phase = MaterialPhase::Empty;
	UInt8 density = 0;                    // higher sinks through lower
	glm::vec3 color{ 0.0f, 0.0f, 0.0f };
	UInt8 flammability = 0;               // permille chance per tick to ignite
	UInt8 lifetime_max = 0;               // burn countdown when ignited
	UInt16 chance_permille = 0;           // deterministic RNG threshold (0..1000)
	UInt8 flags = 0;
MYRENDERER_END_STRUCT

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _MATERIAL_DEF_