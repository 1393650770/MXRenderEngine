#pragma once
#ifndef _PIXEL_WORLD_CONSTANTS_
#define _PIXEL_WORLD_CONSTANTS_

// Shared world constants for the pixel material world.
// IMPORTANT: the GLSL side (resource/Shader/World/world_common.glsl) keeps an
// identical copy via #define - keep both in sync (world size is a power of two
// so the linear index y*W+x is unambiguous on both sides).
//
// Conventions:
//   - 1 cell = 1 world unit. CellFromWorld/WorldFromCell convert.
//   - cell index = y * kWorldW + x

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

static CONST UInt32 kWorldW = 256;
static CONST UInt32 kWorldH = 192;
static CONST UInt32 kMaxMaterials = 256;
static CONST UInt32 kTickRate = 60;

// Empty material always occupies slot 0 (see MaterialDef.h)
static CONST UInt8 kMaterialEmpty = 0;

// Linear cell index. Callers must bound-check x/y BEFORE calling (the
// simulation rules do this via y>0 / x>0 guards); negative inputs are
// clamped to 0 here as a last-resort guard against out-of-bounds reads.
FORCEINLINE UInt32 CellIndex(Int x, Int y)
{
	Int cx = x < 0 ? 0 : (x >= (Int)kWorldW ? (Int)kWorldW - 1 : x);
	Int cy = y < 0 ? 0 : (y >= (Int)kWorldH ? (Int)kWorldH - 1 : y);
	return (UInt32)cy * kWorldW + (UInt32)cx;
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _PIXEL_WORLD_CONSTANTS_