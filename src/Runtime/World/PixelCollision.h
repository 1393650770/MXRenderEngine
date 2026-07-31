#pragma once
#ifndef _PIXEL_COLLISION_
#define _PIXEL_COLLISION_

#include "Core/ConstDefine.h"
#include "World/PixelWorldConstants.h"
#include <glm/glm.hpp>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Read-only collision queries against the CPU solid mask. The mask contains
// ONLY solid materials (stone/wood) - sand/water are passable in Phase 3
// (documented limitation: the GPU authoritative state has no CPU readback,
// so fluid collision requires Phase 4 MapReadback).
//
// Mask update sources (must stay zero-drift with the GPU):
//   (a) every TerrainEditCommand applied through the queue
//   (b) deterministic burn replay: solid flammable cells burn for
//       lifetime_max ticks when ignited (CPU mirrors the GPU reaction with
//       the same rules and tick - no chance-based rules on solids).
MYRENDERER_BEGIN_CLASS(PixelCollision)
#pragma region METHOD
public:
	PixelCollision();
	~PixelCollision() MYDEFAULT;

	void METHOD(SetSolid)(Int x, Int y, Bool solid);
	Bool METHOD(IsSolid)(Int x, Int y) CONST;
	void METHOD(Clear)();

	MYRENDERER_BEGIN_STRUCT(RayHit)
		Bool hit = false;
		Int cell_x = 0;
		Int cell_y = 0;
		Float32 t = 0.0f;
		Int normal_x = 0;
		Int normal_y = 0;
	MYRENDERER_END_STRUCT

	// Amanatides & Woo 2D DDA raycast against solid cells.
	RayHit METHOD(Raycast)(glm::vec2 from, glm::vec2 dir, Float32 max_dist) CONST;
	// Fast circle overlap via per-row u64 bitmasks.
	Bool METHOD(CircleOverlap)(glm::vec2 center, Float32 radius) CONST;
	// Two-axis separated AABB resolution (fixed tick makes it stable).
	void METHOD(ResolveAABB)(glm::vec2& pos, glm::vec2& vel, glm::vec2 half_extents) CONST;

protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	Vector<UInt64> mask_;   // per row: ceil(kWorldW/64) u64 bits

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _PIXEL_COLLISION_