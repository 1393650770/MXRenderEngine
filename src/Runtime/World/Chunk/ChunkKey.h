#pragma once
#ifndef _CHUNK_KEY_
#define _CHUNK_KEY_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

static CONST Int kChunkSize = 64;          // cells per chunk edge
static CONST Int kChunkCellCount = kChunkSize * kChunkSize;

// Chunk coordinate key (integer chunk indices, can be negative).
struct ChunkKey
{
	Int cx = 0;
	Int cy = 0;

	Bool operator==(CONST ChunkKey& rhs) CONST { return cx == rhs.cx && cy == rhs.cy; }
	Bool operator!=(CONST ChunkKey& rhs) CONST { return !(*this == rhs); }
};

struct ChunkKeyHash
{
	size_t operator()(CONST ChunkKey& k) CONST
	{
		return std::hash<Int>{}(k.cx) ^ (std::hash<Int>{}(k.cy) << 1);
	}
};

// Floor division (negative-safe): chunk index for a world cell coordinate.
FORCEINLINE Int CellToChunk(Int cell)
{
	return (cell >= 0) ? (cell / kChunkSize) : -((-cell + kChunkSize - 1) / kChunkSize);
}

// Local cell coordinate within a chunk (0..kChunkSize-1).
FORCEINLINE Int CellToLocal(Int cell)
{
	Int c = cell % kChunkSize;
	return (c < 0) ? (c + kChunkSize) : c;
}

// World cell coordinate from chunk + local.
FORCEINLINE Int ChunkLocalToCell(Int cx, Int local)
{
	return cx * kChunkSize + local;
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _CHUNK_KEY_