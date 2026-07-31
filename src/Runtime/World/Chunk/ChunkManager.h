#pragma once
#ifndef _CHUNK_MANAGER_
#define _CHUNK_MANAGER_

#include "Core/ConstDefine.h"
#include "World/Chunk/ChunkKey.h"
#include "World/Chunk/Chunk.h"
#include "World/Chunk/IChunkGenerator.h"
#include <list>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Owns the loaded chunk set: Map lookup + LRU eviction + pin/refcount.
// GPU atlas slots are tracked here too (free_slots_); the slot assignment
// feeds GpuPixelWorld's chunk parameterization.
MYRENDERER_BEGIN_CLASS(ChunkManager)
#pragma region METHOD
public:
	explicit ChunkManager(UInt32 base_seed);
	~ChunkManager() MYDEFAULT;

	Chunk* METHOD(GetOrCreateChunk)(ChunkKey key);
	Chunk* METHOD(FindChunk)(ChunkKey key) CONST;
	void METHOD(Activate)(ChunkKey key);      // generate -> upload -> Active
	void METHOD(Deactivate)(ChunkKey key);    // readback -> release slot -> Sleeping
	void METHOD(Pin)(ChunkKey key);
	void METHOD(Unpin)(ChunkKey key);
	// Evicts sleeping chunks until under max_memory_bytes (LRU order).
	Int METHOD(UnloadLRU)(Int max_memory_bytes);
	void METHOD(AddGenerator)(UniquePtr<IChunkGenerator> generator);

	UInt32 METHOD(GetActiveChunkCount)() CONST;
	UInt32 METHOD(GetSlotForChunk)(ChunkKey key) CONST;
	ChunkKey METHOD(GetChunkForSlot)(UInt32 slot) CONST;

	// Deterministic per-chunk seed derived from the base seed.
	UInt32 METHOD(GetChunkSeed)(ChunkKey key) CONST;

protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	MYRENDERER_BEGIN_STRUCT(ChunkEntry)
		UniquePtr<Chunk> chunk;
		UInt32 pin_count = 0;
		Int slot = -1;                        // GPU atlas slot, -1 = none
		std::list<ChunkKey>::iterator lru_it;
	MYRENDERER_END_STRUCT

	UInt32 base_seed_ = 0;
	Map<ChunkKey, ChunkEntry, ChunkKeyHash> chunks_;
	std::list<ChunkKey> lru_order_;           // front = least recently used
	Vector<UniquePtr<IChunkGenerator>> generator_stack_;
	Vector<UInt32> free_slots_;               // available GPU atlas slots
	Map<UInt32, ChunkKey> slot_to_key_;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _CHUNK_MANAGER_