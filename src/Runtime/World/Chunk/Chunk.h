#pragma once
#ifndef _CHUNK_
#define _CHUNK_

#include "Core/ConstDefine.h"
#include "World/Chunk/ChunkKey.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

enum class EChunkState : UInt8
{
	Sleeping = 0,   // generated on disk/baseline, no GPU slot
	Loading,        // generating / uploading to GPU slot
	Active,         // simulated on GPU (occupies an atlas slot)
	Unloading       // readback + freeing slot, then Sleeping
};

// A single chunk of the pixel world: packed cells + dirty tracking.
// Cell index within the chunk: local_y * kChunkSize + local_x.
MYRENDERER_BEGIN_CLASS(Chunk)
#pragma region METHOD
public:
	Chunk();
	~Chunk() MYDEFAULT;

	void METHOD(SetCellLocal)(Int lx, Int ly, UInt32 packed);
	UInt32 METHOD(GetCellLocal)(Int lx, Int ly) CONST;
	void METHOD(Fill)(UInt32 packed);
	void METHOD(GetDirtyRect)(Int& out_x0, Int& out_y0, Int& out_x1, Int& out_y1) CONST;
	void METHOD(ClearDirty)();

	EChunkState METHOD(GetState)() CONST { return state_; }
	void METHOD(SetState)(EChunkState state) { state_ = state; }
	Bool METHOD(IsDirty)() CONST { return dirty_x0_ <= dirty_x1_; }

protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	Vector<UInt32> cells_;       // kChunkCellCount packed cells
	EChunkState state_ = EChunkState::Sleeping;
	// Dirty rect (inclusive), invalid when dirty_x0_ > dirty_x1_.
	Int dirty_x0_ = 1;
	Int dirty_y0_ = 0;
	Int dirty_x1_ = 0;
	Int dirty_y1_ = -1;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _CHUNK_