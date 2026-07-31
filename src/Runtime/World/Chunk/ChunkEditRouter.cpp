#include "World/Chunk/ChunkEditRouter.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

void ChunkEditRouter::ApplyEdit(CONST EditEvent& edit)
{
	if (!sink_)
		return;
	// Route to the chunk containing the world cell. If the chunk is not
	// loaded the edit is buffered (assume loaded chunks are those the sink
	// knows about - the caller decides via FlushChunk on activation).
	ChunkKey key;
	key.cx = CellToChunk(edit.x);
	key.cy = CellToChunk(edit.y);
	pending_edits_[key].push_back(edit);
	// NOTE: the router buffers everything; the sink is invoked by FlushChunk
	// after the chunk activates. This keeps edits from being dropped during
	// streaming.
}

void ChunkEditRouter::FlushChunk(ChunkKey key)
{
	if (!sink_)
		return;
	auto it = pending_edits_.find(key);
	if (it == pending_edits_.end())
		return;
	for (CONST auto& edit : it->second)
		sink_->ApplyEdit(edit);
	pending_edits_.erase(it);
}

void ChunkEditRouter::Reset()
{
	pending_edits_.clear();
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender