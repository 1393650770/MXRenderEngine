#pragma once
#ifndef _FRAME_SNAPSHOT_
#define _FRAME_SNAPSHOT_

// FrameSnapshot: the render-minimum view of the logical world, filled at the
// end of each fixed tick (logic thread) and read-only on the render thread.
//
// This is the boundary that makes "logic leads render by 1-2 frames" safe:
// once the lockstep WaitFrameComplete is removed, the render thread can no
// longer reach into live world state (player transforms, cameras, HUD
// fields) - everything it draws must come from here.
//
// Rules:
//   - Fill: logic thread, per tick, via GameWorld::GetWriteSnapshot().
//   - Read: render thread only, via FrameContext::snapshot.
//   - NEVER copy the world (10k entities = disaster). Only the minimum the
//     render thread actually reads belongs here (camera, player quad, HUD).
//   - World-state bulk (terrain, GPU sim) stays on the GPU / retained.

#include "Core/ConstDefine.h"
#include <glm/glm.hpp>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

struct FrameSnapshot
{
	UInt64 frame_tick = 0;

	// Player sprite quad (PlayerPass).
	glm::vec2 player_pos{ 0.0f, 0.0f };
	glm::vec2 player_half_size{ 0.5f, 0.5f };

	// Camera (view-projection baked on the logic thread).
	glm::mat4 camera_mvp{ 1.0f };

	// HUD values: bound UI fields are written on the RENDER thread from these
	// (bound fields become render-thread-exclusive writers - the logic thread
	// only touches the snapshot).
	Int hud_hp = 100;
	Int hud_score = 0;
	Int hud_wand = 0;
};

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender
#endif // _FRAME_SNAPSHOT_
