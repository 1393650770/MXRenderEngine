#pragma once
#ifndef _I_TERRAIN_EDIT_SINK_
#define _I_TERRAIN_EDIT_SINK_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// A single cell edit. `material` is the 8-bit registry slot index.
// `op` is 0 for direct write; OP_DEPOSIT only writes into empty cells.
// Layout is 4 x UInt32 (16 bytes) to match the std430 layout in
// world_apply_edits.comp - keep both sides in sync.
struct EditEvent
{
	Int x = 0;
	Int y = 0;
	UInt32 material = 0;
	UInt32 op = 0;
};

static CONST UInt16 kEditOpWrite = 0;
static CONST UInt16 kEditOpDeposit = 1;

// Adapter interface: every terrain mutation (brush, explosion, projectile
// impact) goes through this sink. CPU backend applies directly; GPU backend
// enqueues into a command buffer (see PixelWorldEventQueue).
MYRENDERER_BEGIN_CLASS(ITerrainEditSink)
#pragma region METHOD
public:
	VIRTUAL ~ITerrainEditSink() MYDEFAULT;
	VIRTUAL void METHOD(ApplyEdit)(CONST EditEvent& edit) PURE;
protected:

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _I_TERRAIN_EDIT_SINK_