#pragma once
#ifndef _FIXED_TICK_CLOCK_
#define _FIXED_TICK_CLOCK_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

// Pure-math fixed-tick accumulator. GameApp calls Advance each frame, then
// PopTicks returns how many simulation ticks to run (0..max). Bounds the
// catch-up to avoid a death spiral on slow frames.
MYRENDERER_BEGIN_CLASS(FixedTickClock)
#pragma region METHOD
public:
	FixedTickClock() MYDEFAULT;
	~FixedTickClock() MYDEFAULT;

	void METHOD(Advance)(Float32 frame_dt);
	// Returns 0..max_ticks ticks to run this frame. Each tick consumes
	// tick_rate seconds from the accumulator.
	UInt32 METHOD(PopTicks)(Float32 tick_rate = 1.0f / 60.0f, UInt32 max_ticks = 4);
	void METHOD(Reset)();
	Float32 METHOD(GetAccumulator)() CONST { return accumulator_; }
protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	Float32 accumulator_ = 0.0f;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Application
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _FIXED_TICK_CLOCK_