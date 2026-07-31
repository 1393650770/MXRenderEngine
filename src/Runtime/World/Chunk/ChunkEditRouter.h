#pragma once
#ifndef _CHUNK_EDIT_ROUTER_
#define _CHUNK_EDIT_ROUTER_

#include "Core/ConstDefine.h"
#include "World/Chunk/ChunkKey.h"
#include "World/ITerrainEditSink.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Routes world-space edits to the correct chunk. Edits landing on a chunk
// that is not loaded (Sleeping/not present) are buffered and flushed when
// the chunk activates - nothing is silently dropped.
MYRENDERER_BEGIN_CLASS(ChunkEditRouter)
#pragma region METHOD
public:
	ChunkEditRouter() MYDEFAULT;
	~ChunkEditRouter() MYDEFAULT;

	void METHOD(SetSink)(ITerrainEditSink* sink) { sink_ = sink; }
	void METHOD(ApplyEdit)(CONST EditEvent& edit);
	// Called by ChunkManager after Activate: flush buffered edits for that
	// chunk into the sink.
	void METHOD(FlushChunk)(ChunkKey key);
	void METHOD(Reset)();

protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	ITerrainEditSink* sink_ = nullptr;
	Map<ChunkKey, Vector<EditEvent>, ChunkKeyHash> pending_edits_;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _CHUNK_EDIT_ROUTER_