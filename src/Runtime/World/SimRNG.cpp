#include "World/SimRNG.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

void SimRNG::Seed(UInt32 seed)
{
	state_ = seed ? seed : 0x9E3779B9u;   // avoid the all-zero state
}

UInt32 SimRNG::Next()
{
	// xorshift32 - deterministic across compilers.
	state_ ^= state_ << 13;
	state_ ^= state_ >> 17;
	state_ ^= state_ << 5;
	return state_;
}

UInt32 SimRNG::HashU32(UInt32 a, UInt32 b)
{
	return WangHash(a ^ (b * kRngPrimeTick));
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender