#pragma once
#ifndef _SIM_RNG_
#define _SIM_RNG_

#include "Core/ConstDefine.h"
#include "World/PixelWorldRng.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Deterministic game-logic RNG (seed-driven). Same Wang-hash family as
// PixelWorldRng so GPU/CPU sides stay consistent. NOT std::mt19937 (its
// output is implementation-defined and breaks save/replay determinism).
MYRENDERER_BEGIN_CLASS(SimRNG)
#pragma region METHOD
public:
	SimRNG() MYDEFAULT;
	~SimRNG() MYDEFAULT;

	void METHOD(Seed)(UInt32 seed);
	UInt32 METHOD(Next)();
	// Deterministic hash of two values (used for procedural generation).
	static UInt32 METHOD(HashU32)(UInt32 a, UInt32 b);

protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	UInt32 state_ = 0;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _SIM_RNG_