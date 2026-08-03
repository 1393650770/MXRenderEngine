#pragma once
#ifndef _LINERENDERER_MANAGER_
#define _LINERENDERER_MANAGER_

// LineRendererManager: unified facade for trail rendering (UIManager-style
// singleton).
//
// Cross-thread model (3-slot value-copy buffer, aligned with the
// FrameSynchronizer back-pressure depth):
//   - Logic thread (GameWorld collect task): GetWriteStates(frame) -> fill
//     LineFrameState copies -> SetWriteCount(frame, n). Slot = frame % 3,
//     before SignalFrameReady (mutex happens-before chain).
//   - Render thread (OnPreRender injection + LinePass execute, same-thread
//     program order): SetRenderFrame(frame, mvp) -> GetRenderCount/States.
//   - Value copies, never component pointers: components die with entities
//     (GarbageCollect) - pointers would dangle on the render thread.
//   - Back-pressure argument: the logic write slot X and the render read
//     slot X never overlap (writing slot (N+3)%3 requires AcquireWriteSlot,
//     which only succeeds after the render thread recycled frame N).

#include "Core/ConstDefine.h"
#include "Render/LineRenderer/LineRendererComponent.h"
#include <glm/glm.hpp>

namespace MXRender {
namespace Render {

class LineRendererManager
{
public:
	static void Create();                    // sample OnGameInit
	static void Destroy();                   // sample OnShutdownScene end (world dies first)
	static LineRendererManager& Get();

	// ---- Logic thread (GameWorld collect task) ----
	// kSlotDepth must equal the FrameSynchronizer in-flight depth.
	enum : UInt32 { kSlotDepth = 3, kMaxRenderers = 64 };
	LineFrameState* GetWriteStates(UInt64 in_frame) { return render_states_[in_frame % kSlotDepth]; }
	UInt32 GetWriteCapacity() const { return kMaxRenderers; }
	void SetWriteCount(UInt64 in_frame, UInt32 in_count)
	{
		render_counts_[in_frame % kSlotDepth] = in_count;
	}

	// ---- Render thread (OnPreRender injection; LinePass execute reads) ----
	void SetRenderFrame(UInt64 in_frame_number, const glm::mat4& in_camera_mvp)
	{
		m_render_frame = in_frame_number;
		m_render_mvp = in_camera_mvp;
	}
	UInt64 GetRenderFrame() const { return m_render_frame; }
	const glm::mat4& GetCameraMVP() const { return m_render_mvp; }
	UInt32 GetRenderCount() const { return render_counts_[m_render_frame % kSlotDepth]; }
	const LineFrameState* GetRenderStates() const { return render_states_[m_render_frame % kSlotDepth]; }

private:
	LineRendererManager() = default;
	~LineRendererManager() = default;
	LineRendererManager(const LineRendererManager&) = delete;
	LineRendererManager& operator=(const LineRendererManager&) = delete;

	LineFrameState render_states_[kSlotDepth][kMaxRenderers]{};   // slot = frame % 3
	UInt32 render_counts_[kSlotDepth] = {};
	UInt64 m_render_frame = 0;              // render-thread exclusive write (OnPreRender)
	glm::mat4 m_render_mvp{ 1.0f };         // render-thread exclusive write

	static LineRendererManager* s_instance;
};

}  // namespace Render
}  // namespace MXRender
#endif // _LINERENDERER_MANAGER_
