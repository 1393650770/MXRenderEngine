#include "World/Chunk/ChunkManager.h"
#include "World/SimRNG.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

ChunkManager::ChunkManager(UInt32 base_seed)
	: base_seed_(base_seed)
{
	// Reserve a reasonable atlas capacity (e.g. 256 chunks).
	free_slots_.reserve(256);
	for (UInt32 i = 0; i < 256; ++i)
		free_slots_.push_back(i);
}

UInt32 ChunkManager::GetChunkSeed(ChunkKey key) CONST
{
	return SimRNG::HashU32(base_seed_, (UInt32)((key.cy << 16) ^ (UInt32)(Int16)key.cx));
}

Chunk* ChunkManager::GetOrCreateChunk(ChunkKey key)
{
	auto it = chunks_.find(key);
	if (it != chunks_.end())
	{
		// LRU touch: move to back (most recently used).
		lru_order_.splice(lru_order_.end(), lru_order_, it->second.lru_it);
		return it->second.chunk.get();
	}

	ChunkEntry entry;
	entry.chunk = std::make_unique<Chunk>();
	entry.pin_count = 0;
	entry.slot = -1;
	lru_order_.push_back(key);
	entry.lru_it = --lru_order_.end();
	chunks_.emplace(key, std::move(entry));
	return chunks_.find(key)->second.chunk.get();
}

Chunk* ChunkManager::FindChunk(ChunkKey key) CONST
{
	auto it = chunks_.find(key);
	return it == chunks_.end() ? nullptr : it->second.chunk.get();
}

void ChunkManager::Activate(ChunkKey key)
{
	auto it = chunks_.find(key);
	if (it == chunks_.end())
	{
		GetOrCreateChunk(key);   // creates Sleeping entry
		it = chunks_.find(key);
	}
	ChunkEntry& entry = it->second;
	if (entry.chunk->GetState() == EChunkState::Active)
		return;
	if (entry.slot < 0)
	{
		if (free_slots_.empty())
			return;   // atlas full; caller retries next frame
		entry.slot = (Int)free_slots_.back();
		free_slots_.pop_back();
		slot_to_key_[(UInt32)entry.slot] = key;
	}

	// Generate baseline (layered generators).
	ChunkBaseline baseline;
	baseline.cells.resize(kChunkCellCount, 0);
	UInt32 chunk_seed = GetChunkSeed(key);
	for (auto& gen : generator_stack_)
	{
		if (gen)
			gen->Generate(key, baseline);
	}
	(void)chunk_seed;

	// Copy baseline into the chunk (edits layer in later).
	for (Int ly = 0; ly < kChunkSize; ++ly)
		for (Int lx = 0; lx < kChunkSize; ++lx)
			entry.chunk->SetCellLocal(lx, ly, baseline.cells[(UInt32)(ly * kChunkSize + lx)]);

	entry.chunk->SetState(EChunkState::Active);
}

void ChunkManager::Deactivate(ChunkKey key)
{
	auto it = chunks_.find(key);
	if (it == chunks_.end())
		return;
	ChunkEntry& entry = it->second;
	if (entry.chunk->GetState() != EChunkState::Active)
		return;
	entry.chunk->SetState(EChunkState::Sleeping);
	if (entry.slot >= 0)
	{
		free_slots_.push_back((UInt32)entry.slot);
		slot_to_key_.erase((UInt32)entry.slot);
		entry.slot = -1;
	}
}

void ChunkManager::Pin(ChunkKey key)
{
	auto it = chunks_.find(key);
	if (it != chunks_.end())
		++it->second.pin_count;
}

void ChunkManager::Unpin(ChunkKey key)
{
	auto it = chunks_.find(key);
	if (it != chunks_.end() && it->second.pin_count > 0)
		--it->second.pin_count;
}

Int ChunkManager::UnloadLRU(Int max_memory_bytes)
{
	// Estimate: packed cells + overhead per chunk.
	Int per_chunk_bytes = (Int)(kChunkCellCount * sizeof(UInt32)) + 256;
	Int total = (Int)chunks_.size() * per_chunk_bytes;
	Int evicted = 0;

	for (auto it = lru_order_.begin(); it != lru_order_.end() && total > max_memory_bytes; )
	{
		ChunkKey key = *it;
		auto cit = chunks_.find(key);
		if (cit == chunks_.end() || cit->second.pin_count > 0 ||
			cit->second.chunk->GetState() != EChunkState::Sleeping)
		{
			++it;
			continue;
		}
		// Evict: remove the entry entirely (re-generable from seed).
		total -= per_chunk_bytes;
		++evicted;
		it = lru_order_.erase(it);
		chunks_.erase(cit);
	}
	return evicted;
}

void ChunkManager::AddGenerator(UniquePtr<IChunkGenerator> generator)
{
	if (generator)
		generator_stack_.push_back(std::move(generator));
}

UInt32 ChunkManager::GetActiveChunkCount() CONST
{
	UInt32 count = 0;
	for (auto& entry : chunks_)
		if (entry.second.chunk->GetState() == EChunkState::Active)
			++count;
	return count;
}

UInt32 ChunkManager::GetSlotForChunk(ChunkKey key) CONST
{
	auto it = chunks_.find(key);
	return it == chunks_.end() ? 0xFFFFFFFFu : (UInt32)it->second.slot;
}

ChunkKey ChunkManager::GetChunkForSlot(UInt32 slot) CONST
{
	auto it = slot_to_key_.find(slot);
	return it == slot_to_key_.end() ? ChunkKey{} : it->second;
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender