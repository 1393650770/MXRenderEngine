#pragma once
#ifndef _NOISE_CAVE_GENERATOR_
#define _NOISE_CAVE_GENERATOR_

#include "World/Chunk/IChunkGenerator.h"
#include "World/PixelWorldConstants.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Deterministic cave generator: value-noise threshold carving.
// Caves are carved where hash-noise exceeds a threshold, leaving stone.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(NoiseCaveGenerator, public IChunkGenerator)
#pragma region METHOD
public:
	NoiseCaveGenerator() MYDEFAULT;
	VIRTUAL void METHOD(Generate)(ChunkKey key, ChunkBaseline& out) OVERRIDE;
protected:

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _NOISE_CAVE_GENERATOR_