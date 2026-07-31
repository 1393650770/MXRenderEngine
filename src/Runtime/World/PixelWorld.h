#pragma once
#ifndef _PIXEL_WORLD_
#define _PIXEL_WORLD_

#include "Core/ConstDefine.h"
#include "World/PixelWorldConstants.h"
#include "World/IPixelWorldSimulator.h"
#include "World/ITerrainEditSink.h"
#include "World/MaterialDef.h"
#include <glm/glm.hpp>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// CPU implementation of the pixel material world (Phase 1).
// Cell-state queries and dirty tracking are CPU-only (GPU has no readback).
// Tick rules: powder falls + side-slides, liquid falls + spreads, fire burns
// neighbors with a lifetime countdown. Update order: bottom-up scan so moved
// material is processed once per tick.
// NOTE: dual-interface class (class macro supports single inheritance only;
// World/ is outside the reflection scan scope, so plain class is fine).
class PixelWorld : public IPixelWorldSimulator, public ITerrainEditSink
{
#pragma region METHOD
public:
	PixelWorld();
	VIRTUAL ~PixelWorld() MYDEFAULT;

	// ---- IPixelWorldSimulator ----
	VIRTUAL void METHOD(TickFrame)(CONST SimFrameContext& ctx) OVERRIDE;
	VIRTUAL UInt32 METHOD(GetWorldWidth)() CONST OVERRIDE { return kWorldW; }
	VIRTUAL UInt32 METHOD(GetWorldHeight)() CONST OVERRIDE { return kWorldH; }
	VIRTUAL UInt64 METHOD(GetTickCount)() CONST OVERRIDE { return tick_count_; }
	VIRTUAL void METHOD(Reset)(UInt32 seed) OVERRIDE;

	// ---- ITerrainEditSink ----
	VIRTUAL void METHOD(ApplyEdit)(CONST EditEvent& edit) OVERRIDE;

	// ---- CPU-only cell queries ----
	void METHOD(SetCell)(Int x, Int y, UInt8 material);
	UInt8 METHOD(GetCell)(Int x, Int y) CONST;
	UInt32 METHOD(GetPackedCell)(Int x, Int y) CONST;
	// Union of all cells touched since the last GetDirtyRect call.
	void METHOD(GetDirtyRect)(Int& out_left, Int& out_bottom, Int& out_right, Int& out_top) CONST;
	void METHOD(ClearDirty)();

	// ---- world <-> cell coordinate conversion (1 cell = 1 world unit) ----
	glm::ivec2 METHOD(CellFromWorld)(glm::vec2 world) CONST;
	glm::vec2 METHOD(WorldFromCell)(glm::ivec2 cell) CONST;

	// For determinism verification (Phase 3): the packed cell array.
	CONST Vector<UInt32>& METHOD(GetCells)() CONST { return cells_; }

protected:
	void METHOD(Tick)();
	void METHOD(TickPowder)(Int x, Int y, UInt8 mat, CONST MaterialDef& def);
	void METHOD(TickLiquid)(Int x, Int y, UInt8 mat, CONST MaterialDef& def);
	void METHOD(TickFire)(Int x, Int y, UInt8 mat, CONST MaterialDef& def);
	void METHOD(MoveCell)(Int sx, Int sy, Int dx, Int dy);
	void METHOD(SetPacked)(Int x, Int y, UInt32 packed);
	void METHOD(MarkDirty)(Int x, Int y);

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	Vector<UInt32> cells_;        // packed cells, kWorldW * kWorldH
	UInt64 tick_count_ = 0;
	Int dirty_left_ = -1;
	Int dirty_bottom_ = -1;
	Int dirty_right_ = -1;
	Int dirty_top_ = -1;

private:
#pragma endregion
};  // class PixelWorld

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _PIXEL_WORLD_