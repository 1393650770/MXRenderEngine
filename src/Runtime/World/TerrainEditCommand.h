#pragma once
#ifndef _TERRAIN_EDIT_COMMAND_
#define _TERRAIN_EDIT_COMMAND_

#include "Core/ConstDefine.h"
#include "World/ITerrainEditSink.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

enum class TerrainEditType : UInt8
{
	Carve = 0,     // remove material inside radius (to Empty)
	Replace = 1,   // set all cells inside radius to material
	Deposit = 2,   // set only empty cells inside radius to material
	Explode = 3    // carve radius + ignite flammable cells
};

// Creation-layer command. TerrainEditQueue expands it into per-cell EditEvents
// on the CPU side (deterministic order + dedup), so the GPU apply shader has
// no op branching and no write conflicts.
MYRENDERER_BEGIN_STRUCT(TerrainEditCommand)
	TerrainEditType type = TerrainEditType::Carve;
	Int x = 0;
	Int y = 0;
	UInt8 material = 0;
	Float32 radius = 3.0f;
MYRENDERER_END_STRUCT

// Command queue: gameplay systems Push() commands; GameWorld flushes them
// into the active simulator's edit sink once per tick. Also maintains the
// CPU solid-mask mirror for deterministic collision (Phase 3).
MYRENDERER_BEGIN_CLASS(TerrainEditQueue)
#pragma region METHOD
public:
	TerrainEditQueue() MYDEFAULT;
	~TerrainEditQueue() MYDEFAULT;

	void METHOD(Push)(CONST TerrainEditCommand& cmd);
	// Expands all pending commands into per-cell EditEvents (deterministic
	// order, last-write-wins dedup) and forwards them to the sink.
	// When out_edits is non-null, every applied edit is appended to it so the
	// caller can mirror the CPU solid mask (Phase 3 collision).
	void METHOD(FlushTo)(ITerrainEditSink& sink, Vector<EditEvent>* out_edits = nullptr);
	void METHOD(Reset)();
	Bool METHOD(HasPending)() CONST { return !commands_.empty(); }

protected:
	void METHOD(ExpandCircle)(CONST TerrainEditCommand& cmd, Map<UInt32, EditEvent>& out_cells);
	UInt32 METHOD(CellKey)(Int x, Int y) CONST;

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	Vector<TerrainEditCommand> commands_;
	static CONST UInt32 kMaxCommands = 4096;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _TERRAIN_EDIT_COMMAND_