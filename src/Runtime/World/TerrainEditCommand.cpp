#include "World/TerrainEditCommand.h"
#include "World/PixelWorldConstants.h"
#include "World/MaterialRegistry.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

void TerrainEditQueue::Push(CONST TerrainEditCommand& cmd)
{
	if (commands_.size() >= kMaxCommands)
		commands_.erase(commands_.begin());   // drop oldest
	commands_.push_back(cmd);
}

UInt32 TerrainEditQueue::CellKey(Int x, Int y) CONST
{
	// Bit-pack cell coords into a single key (world fits in 16 bits each).
	return ((UInt32)(y + 32768) << 16) | (UInt32)(x + 32768);
}

void TerrainEditQueue::ExpandCircle(CONST TerrainEditCommand& cmd, Map<UInt32, EditEvent>& out_cells)
{
	Int r = (Int)cmd.radius;
	UInt8 mat = cmd.material;
	for (Int dy = -r; dy <= r; ++dy)
	{
		for (Int dx = -r; dx <= r; ++dx)
		{
			Float32 dist = std::sqrt((Float32)(dx * dx + dy * dy));
			if (dist > cmd.radius)
				continue;
			Int x = cmd.x + dx;
			Int y = cmd.y + dy;
			if (x < 0 || y < 0 || x >= (Int)kWorldW || y >= (Int)kWorldH)
				continue;

			UInt32 key = CellKey(x, y);
			EditEvent edit;
			edit.x = x;
			edit.y = y;
			edit.material = mat;
			switch (cmd.type)
			{
			case TerrainEditType::Carve:
				edit.material = kMaterialEmpty;
				edit.op = kEditOpWrite;
				break;
			case TerrainEditType::Replace:
				edit.op = kEditOpWrite;
				break;
			case TerrainEditType::Deposit:
				edit.op = kEditOpDeposit;
				break;
			case TerrainEditType::Explode:
				edit.material = kMaterialEmpty;
				edit.op = kEditOpWrite;
				break;
			}
			out_cells[key] = edit;   // last-write-wins dedup
		}
	}
}

void TerrainEditQueue::FlushTo(ITerrainEditSink& sink, Vector<EditEvent>* out_edits)
{
	if (commands_.empty())
		return;

	// Order matters for the last-write-wins dedup: fire FIRST (Deposit, ring
	// of radius+2), then the main commands. If the carve ran first, the fire
	// deposit would overwrite the crater-center cells and, being
	// deposit-only-on-empty, leave the stone intact -> explosions never dig.
	Map<UInt32, EditEvent> cells;
	for (CONST auto& cmd : commands_)
	{
		if (cmd.type == TerrainEditType::Explode)
		{
			TerrainEditCommand fire_cmd;
			fire_cmd.type = TerrainEditType::Deposit;
			fire_cmd.x = cmd.x;
			fire_cmd.y = cmd.y;
			fire_cmd.material = MaterialRegistry::GetFire();
			fire_cmd.radius = cmd.radius + 2.0f;
			ExpandCircle(fire_cmd, cells);
		}
	}
	for (CONST auto& cmd : commands_)
		ExpandCircle(cmd, cells);

	// Single pass: apply every unique cell once.
	if (out_edits)
		out_edits->reserve(out_edits->size() + cells.size());
	for (CONST auto& entry : cells)
	{
		sink.ApplyEdit(entry.second);
		if (out_edits)
			out_edits->push_back(entry.second);
	}

	commands_.clear();
}

void TerrainEditQueue::Reset()
{
	commands_.clear();
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender