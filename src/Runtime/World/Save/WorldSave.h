#pragma once
#ifndef _WORLD_SAVE_
#define _WORLD_SAVE_

#include "Core/ConstDefine.h"
#include "World/Chunk/ChunkKey.h"
#include "World/Save/PersistentEntityID.h"
#include "World/ITerrainEditSink.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// One chunk's saved state: baseline-version + per-cell diffs (varint-encoded
// index deltas) compressed with LZ4. Chunk cell data is NOT the full world -
// it is the difference from the deterministic procedural baseline, so saves
// stay small even for large worlds.
MYRENDERER_BEGIN_STRUCT(ChunkDelta)
	ChunkKey key;
	UInt32 baseline_version = 0;
	Vector<UInt8> compressed_delta;   // varint index deltas + packed values
	Bool has_delta = false;           // false = chunk matches baseline exactly
MYRENDERER_END_STRUCT

// World save document: metadata + chunk deltas + edit log (debug/verify).
MYRENDERER_BEGIN_STRUCT(WorldSave)
	UInt32 format_version = 1;
	UInt32 seed = 0;
	UInt64 tick = 0;
	UInt32 material_version = 1;
	UInt32 generation_version = 1;
	UInt64 next_entity_id = 1;
	Vector<ChunkDelta> chunk_deltas;
	Vector<EditEvent> edit_log;       // debug/verification only
MYRENDERER_END_STRUCT

// ---- save file IO ----
void METHOD(SaveWorldToFile)(CONST WorldSave& save, CONST String& path);
Bool METHOD(LoadWorldFromFile)(CONST String& path, WorldSave& out);

// ---- delta encode/decode (used by SaveManager) ----
Vector<UInt8> METHOD(EncodeChunkDelta)(CONST Vector<UInt32>& cells,
	CONST Vector<UInt32>& baseline, Bool& has_delta);
Bool METHOD(DecodeChunkDelta)(CONST Vector<UInt8>& compressed, UInt32 cell_count,
	CONST Vector<UInt32>& baseline, Vector<UInt32>& out);

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _WORLD_SAVE_