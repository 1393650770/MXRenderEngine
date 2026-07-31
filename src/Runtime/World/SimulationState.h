#pragma once
#ifndef _SIMULATION_STATE_
#define _SIMULATION_STATE_

#include "Core/ConstDefine.h"
#include "World/ITerrainEditSink.h"
#include "World/SimRNG.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Pure-data simulation state, owned by GameWorld (separated from subsystem
// ownership so it can be serialized whole for save/load).
MYRENDERER_BEGIN_STRUCT(SimulationState)
	UInt64 tick = 0;
	UInt32 seed = 0;
	SimRNG rng;
	Vector<EditEvent> edit_log;      // full edit history (save replay/debug)
	UInt64 next_entity_id = 1;       // persistent entity ID counter
MYRENDERER_END_STRUCT

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _SIMULATION_STATE_