#include "World/Chunk/Generators/RoomGenerator.h"
#include "World/CellPacking.h"
#include "World/MaterialRegistry.h"
#include "World/SimRNG.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

void RoomGenerator::Generate(ChunkKey key, ChunkBaseline& out)
{
	// ~1 in 5 chunks gets a room (deterministic per chunk).
	UInt32 seed = SimRNG::HashU32(0x80AAu, (UInt32)(key.cy * 100000 + key.cx));
	if ((WangHash(seed) & 0xFFu) > 51u)
		return;   // no room in this chunk

	// Room: 20x12 centered, random offset.
	UInt32 rx = 10u + (WangHash(seed ^ 0x1111u) & 0x1Fu);
	UInt32 ry = 8u + (WangHash(seed ^ 0x2222u) & 0xFu);
	UInt32 rw = 20u;
	UInt32 rh = 12u;

	UInt8 empty = kMaterialEmpty;
	UInt32 empty_packed = PackCell(empty, (UInt8)kTempZeroOffset, 0, 0);

	for (UInt32 ly = ry; ly < ry + rh && ly < (UInt32)kChunkSize; ++ly)
	{
		for (UInt32 lx = rx; lx < rx + rw && lx < (UInt32)kChunkSize; ++lx)
			out.cells[(UInt32)(ly * kChunkSize + lx)] = empty_packed;
	}

	// Corridor to the left edge at mid-height.
	for (UInt32 lx = 0; lx < rx; ++lx)
		out.cells[(UInt32)((ry + rh / 2) * kChunkSize + lx)] = empty_packed;
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender