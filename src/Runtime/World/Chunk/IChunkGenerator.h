#pragma once
#ifndef _I_CHUNK_GENERATOR_
#define _I_CHUNK_GENERATOR_

#include "Core/ConstDefine.h"
#include "World/Chunk/ChunkKey.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Deterministic procedural baseline for a chunk (before player edits).
MYRENDERER_BEGIN_STRUCT(ChunkBaseline)
	Vector<UInt32> cells;   // kChunkCellCount packed cells
MYRENDERER_END_STRUCT

// Strategy: a chunk generator fills the baseline for one chunk. Generators
// are layered (surface -> caves -> lava) and applied in order by
// ChunkManager. Determinism contract: same (base_seed, ChunkKey) always
// produces the same baseline - no std::rand, use hash-derived pseudo-random.
MYRENDERER_BEGIN_CLASS(IChunkGenerator)
#pragma region METHOD
public:
	VIRTUAL ~IChunkGenerator() MYDEFAULT;
	VIRTUAL void METHOD(Generate)(ChunkKey key, ChunkBaseline& out) PURE;
protected:

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _I_CHUNK_GENERATOR_