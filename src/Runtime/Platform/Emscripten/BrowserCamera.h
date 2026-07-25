#pragma once
#ifndef _BROWSER_CAMERA_
#define _BROWSER_CAMERA_

#if PLATFORM_GLES3

#include "Application/MiniGameCamera.h"
#include "Input/InputSystem.h"
#include <glm/glm.hpp>
#include <cmath>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Platform)
MYRENDERER_BEGIN_NAMESPACE(Emscripten)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(BrowserCamera, public Application::MiniGameCamera)
#pragma region METHOD
public:
	BrowserCamera() MYDEFAULT;
	VIRTUAL ~BrowserCamera() MYDEFAULT;
	VIRTUAL void METHOD(Update)(Float32 dt, UInt32 vp_w, UInt32 vp_h, Render::SceneView& out_view) OVERRIDE FINAL;
	VIRTUAL void METHOD(Reset)() OVERRIDE FINAL;
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

inline void BrowserCamera::Update(Float32 dt, UInt32 vp_w, UInt32 vp_h, Render::SceneView& out_view)
{
	auto& input = Input::InputSystem::Get();
	Float32 dx, dy;
	input.GetMouseDelta(dx, dy);
	Float32 scroll = input.GetScrollDelta();

	if (input.IsMouseDown(0)) {
		yaw   += dx * rotate_speed;
		pitch -= dy * rotate_speed;
		pitch = glm::clamp(pitch, -1.45f, 1.45f);
	}
	if (scroll != 0.0f) {
		distance -= scroll * zoom_speed * 0.1f;
		distance = glm::clamp(distance, min_dist, max_dist);
	}

	CONST auto& touch = input.GetTouch();
	static float tx_last = 0, ty_last = 0, last_pinch = 0;
	static bool touch_active = false;

	if (touch.pointer_count == 1 && touch.pointers[0].active) {
		if (!touch_active) { tx_last = touch.pointers[0].x; ty_last = touch.pointers[0].y; touch_active = true; }
		float tdx = touch.pointers[0].x - tx_last;
		float tdy = touch.pointers[0].y - ty_last;
		yaw   += tdx * rotate_speed;
		pitch -= tdy * rotate_speed;
		pitch = glm::clamp(pitch, -1.45f, 1.45f);
		tx_last = touch.pointers[0].x; ty_last = touch.pointers[0].y;
	} else { touch_active = false; last_pinch = 0; }

	if (touch.pointer_count >= 2) {
		float d = sqrtf(powf(touch.pointers[0].x - touch.pointers[1].x, 2) +
		                powf(touch.pointers[0].y - touch.pointers[1].y, 2));
		if (last_pinch > 0) { distance *= (last_pinch / d); distance = glm::clamp(distance, min_dist, max_dist); }
		last_pinch = d;
	}

	float cx = target.x + distance * cosf(pitch) * sinf(yaw);
	float cy = target.y + distance * sinf(pitch);
	float cz = target.z + distance * cosf(pitch) * cosf(yaw);

	out_view.SetViewport(vp_w, vp_h);
	out_view.SetViewLookAt(glm::vec3(cx, cy, cz), target, glm::vec3(0, 1, 0));
	out_view.UpdateMatrices();
}

inline void BrowserCamera::Reset()
{
	yaw = 0.7f; pitch = 0.25f; distance = 6.0f;
	target = glm::vec3(0, 0, 0);
}

MYRENDERER_END_NAMESPACE  // Emscripten
MYRENDERER_END_NAMESPACE  // Platform
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _BROWSER_CAMERA_
