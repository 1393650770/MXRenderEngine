#pragma once
#ifndef _PIXEL_WORLD_RNG_
#define _PIXEL_WORLD_RNG_

// Deterministic integer RNG shared by CPU (this header) and GPU
// (resource/Shader/World/world_common.glsl). Both sides must produce
// bit-identical results - keep the constants and operations in sync.
//
// Do NOT use std::mt19937 here: its output is implementation-defined.
// Threshold comparisons must use integer permille, never floats.

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

static CONST UInt32 kRngPrimeTick = 0x9E3779B9u;
static CONST UInt32 kRngPrimeSeed = 0x85EBCA77u;

FORCEINLINE UInt32 WangHash(UInt32 key)
{
	key = (key ^ 61u) ^ (key >> 16);
	key = key + (key << 3u);
	key = key ^ (key >> 4u);
	key = key * 0x27d4eb2du;
	key = key ^ (key >> 15u);
	return key;
}

// One 0..1023 dice per cell per tick.
FORCEINLINE UInt32 RngDice(UInt32 cell_index, UInt32 tick, UInt32 material_seed)
{
	UInt32 key = cell_index ^ (tick * kRngPrimeTick) ^ (material_seed * kRngPrimeSeed);
	return WangHash(key) & 0x3FFu;
}

// Deterministic hit test: chance_permille in [0, 1000].
FORCEINLINE Bool RngHit(UInt32 cell_index, UInt32 tick, UInt32 material_seed, UInt32 chance_permille)
{
	return RngDice(cell_index, tick, material_seed) < chance_permille;
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _PIXEL_WORLD_RNG_