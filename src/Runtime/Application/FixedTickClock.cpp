#include "Application/FixedTickClock.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

void FixedTickClock::Advance(Float32 frame_dt)
{
	accumulator_ += frame_dt;
}

UInt32 FixedTickClock::PopTicks(Float32 tick_rate, UInt32 max_ticks)
{
	UInt32 ticks = 0;
	while (accumulator_ >= tick_rate && ticks < max_ticks)
	{
		accumulator_ -= tick_rate;
		++ticks;
	}
	return ticks;
}

void FixedTickClock::Reset()
{
	accumulator_ = 0.0f;
}

MYRENDERER_END_NAMESPACE  // Application
MYRENDERER_END_NAMESPACE  // MXRender