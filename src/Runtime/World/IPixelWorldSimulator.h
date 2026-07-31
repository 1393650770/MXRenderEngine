#pragma once
#ifndef _I_PIXEL_WORLD_SIMULATOR_
#define _I_PIXEL_WORLD_SIMULATOR_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class CommandList;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Strategy interface for the pixel world simulator. Two implementations:
//   - PixelWorld     (CPU, Phase 1)
//   - GpuPixelWorld  (GPU SSBO authoritative, Phase 2)
// The interface is intentionally narrow: GPU has no readback, so cell-state
// queries live on the concrete CPU class, not here.
MYRENDERER_BEGIN_CLASS(IPixelWorldSimulator)
#pragma region METHOD
public:
	VIRTUAL ~IPixelWorldSimulator() MYDEFAULT;

	// Per-frame context handed to TickFrame. pending_ticks comes from the
	// fixed-tick accumulator (0..4); frame_index selects the double-buffered
	// event buffer parity.
	MYRENDERER_BEGIN_STRUCT(SimFrameContext)
		RHI::CommandList* cmd = nullptr;
		UInt32 pending_ticks = 0;
		UInt32 frame_index = 0;
		UInt32 tick_count = 0;
	MYRENDERER_END_STRUCT

	VIRTUAL void METHOD(TickFrame)(CONST SimFrameContext& ctx) PURE;
	VIRTUAL UInt32 METHOD(GetWorldWidth)() CONST PURE;
	VIRTUAL UInt32 METHOD(GetWorldHeight)() CONST PURE;
	VIRTUAL UInt64 METHOD(GetTickCount)() CONST PURE;
	// Rebuild the world from a seed (used by GameWorld::Reset on death).
	VIRTUAL void METHOD(Reset)(UInt32 seed) PURE;

protected:

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _I_PIXEL_WORLD_SIMULATOR_