#pragma once
#ifndef _PERSISTENT_ENTITY_ID_
#define _PERSISTENT_ENTITY_ID_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Stable entity identity across save/load boundaries. Assigned from a
// monotonic counter (GameWorld::SimulationState.next_entity_id) - NOT
// derived from runtime handles (which are registry-slot bound and can be
// reused after destruction).
struct PersistentEntityID
{
	UInt64 value = 0;

	Bool operator==(CONST PersistentEntityID& rhs) CONST { return value == rhs.value; }
	Bool operator!=(CONST PersistentEntityID& rhs) CONST { return value != rhs.value; }
	Bool IsValid() CONST { return value != 0; }
};

struct PersistentEntityIDHash
{
	size_t operator()(CONST PersistentEntityID& id) CONST { return std::hash<UInt64>{}(id.value); }
};

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _PERSISTENT_ENTITY_ID_