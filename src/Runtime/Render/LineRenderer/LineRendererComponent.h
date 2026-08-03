#pragma once
#ifndef _LINERENDERER_COMPONENT_
#define _LINERENDERER_COMPONENT_

// LineRendererComponent: Unity LineRenderer-style trail ECS component (plain
// data, EnTT compatible: default-constructible + trivially copyable). Attach
// to an entity; the trail follows the entity lifecycle (entity destroyed ->
// component destroyed -> trail vanishes, natural component semantics).
//
// Cross-thread model (snapshot-protocol compliant):
//   - This component is the "logic-side authoritative data" (logic thread
//     exclusive writes; the render thread never touches it);
//   - The render view is produced by the GameWorld collect task (tick-graph
//     tail): value-copies into LineRendererManager's 3 slots
//     (LineFrameState); the render thread reads slots only;
//   - Slot depth 3 matches the FrameSynchronizer in-flight depth (see the
//     back-pressure argument in LineRendererManager.h - do not change).

#include "Core/ConstDefine.h"
#include <glm/glm.hpp>
#include <cstring>

namespace MXRender {
namespace Render {

// Render-thread-visible trail state (value-copied from components at collect
// time; render reads the slot for its frame number, never the component).
struct LineFrameState
{
	glm::vec2 points[64]{};
	UInt16    point_count = 0;
	glm::vec4 start_color{ 1.0f, 1.0f, 1.0f, 1.0f };
	glm::vec4 end_color{ 1.0f, 1.0f, 1.0f, 1.0f };
	Float32   start_width = 1.0f;
	Float32   end_width = 1.0f;
	Bool      loop = false;
	Bool      visible = true;
};

struct LineRendererComponent
{
	static constexpr UInt32 kMaxPoints = 64;   // per-trail point cap (oldest dropped when full)

	// ---- Logic-side authoritative data (logic thread exclusive writes) ----
	glm::vec2 history[kMaxPoints]{};
	UInt16    history_count = 0;
	glm::vec4 start_color{ 1.0f, 1.0f, 1.0f, 1.0f };
	glm::vec4 end_color{ 1.0f, 1.0f, 1.0f, 1.0f };
	Float32   start_width = 1.0f;
	Float32   end_width = 1.0f;
	Bool      loop = false;
	Bool      visible = true;

	// ---- Unity-style API (logic thread; all inline, pure logic) ----
	void SetPositionCount(UInt32 in_count)
	{
		history_count = (UInt16)(in_count < kMaxPoints ? in_count : kMaxPoints);
	}
	UInt32 GetPositionCount() const { return history_count; }
	void SetPosition(UInt32 in_index, const glm::vec2& in_pos)
	{
		if (in_index < kMaxPoints)
		{
			history[in_index] = in_pos;
			if (history_count <= in_index)
				history_count = (UInt16)(in_index + 1);
		}
	}
	void SetPositions(const glm::vec2* in_positions, UInt32 in_count)
	{
		UInt32 n = in_count < kMaxPoints ? in_count : kMaxPoints;
		std::memcpy(history, in_positions, n * sizeof(glm::vec2));
		history_count = (UInt16)n;
	}
	// Trail append: drop the oldest when full (ring overwrite; 512B memmove
	// per tick is negligible).
	void AddPoint(const glm::vec2& in_pos)
	{
		if (history_count == kMaxPoints)
		{
			std::memmove(history, history + 1, (kMaxPoints - 1) * sizeof(glm::vec2));
			history_count = kMaxPoints - 1;
		}
		history[history_count++] = in_pos;
	}
	glm::vec2 GetPosition(UInt32 in_index) const { return history[in_index]; }
	void Clear() { history_count = 0; }
	void SetWidth(Float32 in_start, Float32 in_end) { start_width = in_start; end_width = in_end; }
	void SetColor(const glm::vec4& in_start, const glm::vec4& in_end) { start_color = in_start; end_color = in_end; }
	void SetLoop(Bool in_loop) { loop = in_loop; }
	void SetVisible(Bool in_visible) { visible = in_visible; }
};

// Collect-time conversion (logic thread): component -> render view (value
// copy; the render thread never touches the component).
inline LineFrameState MakeLineFrameState(const LineRendererComponent& in_comp)
{
	LineFrameState st{};
	std::memcpy(st.points, in_comp.history, in_comp.history_count * sizeof(glm::vec2));
	st.point_count = in_comp.history_count;
	st.start_color = in_comp.start_color;
	st.end_color = in_comp.end_color;
	st.start_width = in_comp.start_width;
	st.end_width = in_comp.end_width;
	st.loop = in_comp.loop;
	st.visible = in_comp.visible;
	return st;
}

}  // namespace Render
}  // namespace MXRender
#endif // _LINERENDERER_COMPONENT_
