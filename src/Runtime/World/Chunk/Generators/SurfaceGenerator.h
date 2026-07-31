#pragma once
#ifndef _SURFACE_GENERATOR_
#define _SURFACE_GENERATOR_

#include "World/Chunk/IChunkGenerator.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Fills the top region of the chunk with stone and the air above with Empty.
// Produces a flat-ish terrain surface at a configurable height offset.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(SurfaceGenerator, public IChunkGenerator)
#pragma region METHOD
public:
	explicit SurfaceGenerator(Int surface_world_y = 0);
	VIRTUAL void METHOD(Generate)(ChunkKey key, ChunkBaseline& out) OVERRIDE;
protected:

private:
	Int surface_world_y_ = 0;
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _SURFACE_GENERATOR_