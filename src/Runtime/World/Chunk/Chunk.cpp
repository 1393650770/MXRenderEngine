#include "World/Chunk/Chunk.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

Chunk::Chunk()
{
	cells_.resize(kChunkCellCount, 0);
}

void Chunk::SetCellLocal(Int lx, Int ly, UInt32 packed)
{
	if (lx < 0 || ly < 0 || lx >= kChunkSize || ly >= kChunkSize)
		return;
	cells_[(UInt32)(ly * kChunkSize + lx)] = packed;
	if (dirty_x0_ > dirty_x1_)
	{
		dirty_x0_ = dirty_x1_ = lx;
		dirty_y0_ = dirty_y1_ = ly;
	}
	else
	{
		if (lx < dirty_x0_) dirty_x0_ = lx;
		if (lx > dirty_x1_) dirty_x1_ = lx;
		if (ly < dirty_y0_) dirty_y0_ = ly;
		if (ly > dirty_y1_) dirty_y1_ = ly;
	}
}

UInt32 Chunk::GetCellLocal(Int lx, Int ly) CONST
{
	if (lx < 0 || ly < 0 || lx >= kChunkSize || ly >= kChunkSize)
		return 0;
	return cells_[(UInt32)(ly * kChunkSize + lx)];
}

void Chunk::Fill(UInt32 packed)
{
	std::fill(cells_.begin(), cells_.end(), packed);
	dirty_x0_ = 0;
	dirty_y0_ = 0;
	dirty_x1_ = kChunkSize - 1;
	dirty_y1_ = kChunkSize - 1;
}

void Chunk::GetDirtyRect(Int& out_x0, Int& out_y0, Int& out_x1, Int& out_y1) CONST
{
	out_x0 = dirty_x0_;
	out_y0 = dirty_y0_;
	out_x1 = dirty_x1_;
	out_y1 = dirty_y1_;
}

void Chunk::ClearDirty()
{
	dirty_x0_ = 1;
	dirty_y0_ = 0;
	dirty_x1_ = 0;
	dirty_y1_ = -1;
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender