#include "World/PixelWorld.h"
#include "World/CellPacking.h"
#include "World/MaterialRegistry.h"
#include "World/PixelWorldRng.h"
#include <algorithm>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

PixelWorld::PixelWorld()
{
	cells_.resize(kWorldW * kWorldH, PackCell(kMaterialEmpty, kTempZeroOffset, 0, 0));
}

void PixelWorld::Reset(UInt32 seed)
{
	(void)seed;   // Phase 1 has no procedural generation; a future generator
	              // will consume the seed to rebuild the baseline terrain.
	std::fill(cells_.begin(), cells_.end(), PackCell(kMaterialEmpty, kTempZeroOffset, 0, 0));
	tick_count_ = 0;
	ClearDirty();
}

void PixelWorld::TickFrame(CONST SimFrameContext& ctx)
{
	(void)ctx.cmd;
	(void)ctx.frame_index;
	UInt32 n = ctx.pending_ticks;
	if (n == 0)
		return;
	for (UInt32 t = 0; t < n; ++t)
		Tick();
}

void PixelWorld::Tick()
{
	++tick_count_;

	// Bottom-up scan: material settles before its row is processed.
	for (Int y = (Int)kWorldH - 1; y >= 0; --y)
	{
		for (Int x = 0; x < (Int)kWorldW; ++x)
		{
			UInt32 packed = cells_[CellIndex(x, y)];
			UInt8 mat = UnpackMat(packed);
			if (mat == kMaterialEmpty)
				continue;

			const MaterialDef& def = MaterialRegistry::Get(mat);
			switch (def.phase)
			{
			case MaterialPhase::Powder:
				TickPowder(x, y, mat, def);
				break;
			case MaterialPhase::Liquid:
				TickLiquid(x, y, mat, def);
				break;
			case MaterialPhase::Fire:
				TickFire(x, y, mat, def);
				break;
			default:
				break;   // Solid / Empty: static this phase
			}
		}
	}
}

void PixelWorld::TickPowder(Int x, Int y, UInt8 mat, CONST MaterialDef& def)
{
	// Try straight down, then side-slide (deterministic order: down-left first).
	if (y > 0 && UnpackMat(cells_[CellIndex(x, y - 1)]) == kMaterialEmpty)
	{
		MoveCell(x, y, x, y - 1);
		return;
	}
	if (y > 0 && x > 0 && UnpackMat(cells_[CellIndex(x - 1, y - 1)]) == kMaterialEmpty)
	{
		MoveCell(x, y, x - 1, y - 1);
		return;
	}
	if (y > 0 && x < (Int)kWorldW - 1 && UnpackMat(cells_[CellIndex(x + 1, y - 1)]) == kMaterialEmpty)
	{
		MoveCell(x, y, x + 1, y - 1);
		return;
	}
	// Also slide horizontally when resting on a solid support.
	if (y > 0 && x > 0 && UnpackMat(cells_[CellIndex(x - 1, y)]) == kMaterialEmpty &&
		UnpackMat(cells_[CellIndex(x - 1, y - 1)]) != kMaterialEmpty)
	{
		MoveCell(x, y, x - 1, y);
		return;
	}
	if (y > 0 && x < (Int)kWorldW - 1 && UnpackMat(cells_[CellIndex(x + 1, y)]) == kMaterialEmpty &&
		UnpackMat(cells_[CellIndex(x + 1, y - 1)]) != kMaterialEmpty)
	{
		MoveCell(x, y, x + 1, y);
		return;
	}
	(void)def;
}

void PixelWorld::TickLiquid(Int x, Int y, UInt8 mat, CONST MaterialDef& def)
{
	// Down, then diagonal down, then horizontal spread (limited to 2 cells/tick).
	if (y > 0 && UnpackMat(cells_[CellIndex(x, y - 1)]) == kMaterialEmpty)
	{
		MoveCell(x, y, x, y - 1);
		return;
	}
	if (y > 0 && x > 0 && UnpackMat(cells_[CellIndex(x - 1, y - 1)]) == kMaterialEmpty)
	{
		MoveCell(x, y, x - 1, y - 1);
		return;
	}
	if (y > 0 && x < (Int)kWorldW - 1 && UnpackMat(cells_[CellIndex(x + 1, y - 1)]) == kMaterialEmpty)
	{
		MoveCell(x, y, x + 1, y - 1);
		return;
	}
	// Horizontal spread: prefer the side with more room (deterministic tie -> left).
	if (x > 0 && UnpackMat(cells_[CellIndex(x - 1, y)]) == kMaterialEmpty)
	{
		MoveCell(x, y, x - 1, y);
		return;
	}
	if (x < (Int)kWorldW - 1 && UnpackMat(cells_[CellIndex(x + 1, y)]) == kMaterialEmpty)
	{
		MoveCell(x, y, x + 1, y);
		return;
	}
	(void)def;
}

void PixelWorld::TickFire(Int x, Int y, UInt8 mat, CONST MaterialDef& def)
{
	// Decrement lifetime; when exhausted convert to Ember.
	UInt32 packed = cells_[CellIndex(x, y)];
	UInt8 life = UnpackLife(packed);
	if (life == 0)
	{
		// First fire tick: initialize the countdown.
		SetPacked(x, y, PackCell(mat, UnpackTemp(packed), def.lifetime_max, UnpackFlag(packed)));
		return;
	}
	if (life <= 1)
	{
		SetPacked(x, y, PackCell(MaterialRegistry::GetEmber(), UnpackTemp(packed), 0, UnpackFlag(packed) & ~kCellFlagBurning));
		return;
	}
	SetPacked(x, y, PackCell(mat, UnpackTemp(packed), (UInt8)(life - 1), UnpackFlag(packed)));

	// Ignite flammable neighbors (deterministic RNG).
	UInt32 cell_index = CellIndex(x, y);
	const Int dx[4] = { 1, -1, 0, 0 };
	const Int dy[4] = { 0, 0, 1, -1 };
	for (Int i = 0; i < 4; ++i)
	{
		Int nx = x + dx[i];
		Int ny = y + dy[i];
		if (nx < 0 || ny < 0 || nx >= (Int)kWorldW || ny >= (Int)kWorldH)
			continue;
		UInt32 np = cells_[CellIndex(nx, ny)];
		UInt8 nmat = UnpackMat(np);
		if (nmat == kMaterialEmpty)
			continue;
		const MaterialDef& ndef = MaterialRegistry::Get(nmat);
		if (ndef.flammability == 0)
			continue;
		// Deterministic: hash(cell, tick) < flammability
		if (RngHit(CellIndex(nx, ny), (UInt32)tick_count_, nmat, ndef.flammability))
		{
			SetPacked(nx, ny, PackCell(MaterialRegistry::GetFire(), UnpackTemp(np), ndef.lifetime_max,
				UnpackFlag(np) | kCellFlagBurning));
		}
	}
}

void PixelWorld::MoveCell(Int sx, Int sy, Int dx, Int dy)
{
	UInt32 packed = cells_[CellIndex(sx, sy)];
	cells_[CellIndex(dx, dy)] = packed;
	cells_[CellIndex(sx, sy)] = PackCell(kMaterialEmpty, UnpackTemp(packed), 0, 0);
	MarkDirty(sx, sy);
	MarkDirty(dx, dy);
}

void PixelWorld::SetPacked(Int x, Int y, UInt32 packed)
{
	cells_[CellIndex(x, y)] = packed;
	MarkDirty(x, y);
}

void PixelWorld::MarkDirty(Int x, Int y)
{
	if (dirty_left_ < 0)
	{
		dirty_left_ = dirty_right_ = x;
		dirty_bottom_ = dirty_top_ = y;
		return;
	}
	dirty_left_ = std::min(dirty_left_, x);
	dirty_right_ = std::max(dirty_right_, x);
	dirty_bottom_ = std::min(dirty_bottom_, y);
	dirty_top_ = std::max(dirty_top_, y);
}

void PixelWorld::SetCell(Int x, Int y, UInt8 material)
{
	if (x < 0 || y < 0 || x >= (Int)kWorldW || y >= (Int)kWorldH)
		return;
	SetPacked(x, y, PackCell(material, kTempZeroOffset, 0, 0));
}

UInt8 PixelWorld::GetCell(Int x, Int y) CONST
{
	if (x < 0 || y < 0 || x >= (Int)kWorldW || y >= (Int)kWorldH)
		return kMaterialEmpty;
	return UnpackMat(cells_[CellIndex(x, y)]);
}

UInt32 PixelWorld::GetPackedCell(Int x, Int y) CONST
{
	if (x < 0 || y < 0 || x >= (Int)kWorldW || y >= (Int)kWorldH)
		return PackCell(kMaterialEmpty, kTempZeroOffset, 0, 0);
	return cells_[CellIndex(x, y)];
}

void PixelWorld::ApplyEdit(CONST EditEvent& edit)
{
	if (edit.x < 0 || edit.y < 0 || edit.x >= (Int)kWorldW || edit.y >= (Int)kWorldH)
		return;
	UInt8 mat = (UInt8)(edit.material & 0xFF);
	if (edit.op == kEditOpDeposit && GetCell(edit.x, edit.y) != kMaterialEmpty)
		return;
	SetCell(edit.x, edit.y, mat);
}

void PixelWorld::GetDirtyRect(Int& out_left, Int& out_bottom, Int& out_right, Int& out_top) CONST
{
	out_left = dirty_left_ < 0 ? 0 : dirty_left_;
	out_bottom = dirty_bottom_ < 0 ? 0 : dirty_bottom_;
	out_right = dirty_right_ < 0 ? 0 : dirty_right_;
	out_top = dirty_top_ < 0 ? 0 : dirty_top_;
}

void PixelWorld::ClearDirty()
{
	dirty_left_ = dirty_bottom_ = dirty_right_ = dirty_top_ = -1;
}

glm::ivec2 PixelWorld::CellFromWorld(glm::vec2 world) CONST
{
	return glm::ivec2((Int)std::floor(world.x), (Int)std::floor(world.y));
}

glm::vec2 PixelWorld::WorldFromCell(glm::ivec2 cell) CONST
{
	return glm::vec2((Float32)cell.x + 0.5f, (Float32)cell.y + 0.5f);
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender