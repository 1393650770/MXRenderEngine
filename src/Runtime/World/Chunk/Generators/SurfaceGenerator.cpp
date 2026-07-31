#include "World/Chunk/Generators/SurfaceGenerator.h"
#include "World/CellPacking.h"
#include "World/MaterialRegistry.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

SurfaceGenerator::SurfaceGenerator(Int surface_world_y)
	: surface_world_y_(surface_world_y)
{
}

void SurfaceGenerator::Generate(ChunkKey key, ChunkBaseline& out)
{
	UInt8 stone = MaterialRegistry::GetStone();
	UInt8 empty = kMaterialEmpty;

	for (Int ly = 0; ly < kChunkSize; ++ly)
	{
		Int world_y = key.cy * kChunkSize + ly;
		UInt8 mat = (world_y < surface_world_y_) ? stone : empty;
		UInt32 packed = PackCell(mat, (UInt8)kTempZeroOffset, 0, 0);
		for (Int lx = 0; lx < kChunkSize; ++lx)
			out.cells[(UInt32)(ly * kChunkSize + lx)] = packed;
	}
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender