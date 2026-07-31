#pragma once
#ifndef _ROOM_GENERATOR_
#define _ROOM_GENERATOR_

#include "World/Chunk/IChunkGenerator.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Deterministic room generator: carves a random room (Empty) inside the chunk
// when the chunk seed hash matches. Rooms connect via one corridor to the
// chunk edge. Overlay generator - leaves existing cells untouched outside
// the room.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RoomGenerator, public IChunkGenerator)
#pragma region METHOD
public:
	RoomGenerator() MYDEFAULT;
	VIRTUAL void METHOD(Generate)(ChunkKey key, ChunkBaseline& out) OVERRIDE;
protected:

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _ROOM_GENERATOR_