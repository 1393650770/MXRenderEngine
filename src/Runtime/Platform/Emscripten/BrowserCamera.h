#pragma once
#ifndef _BROWSER_CAMERA_
#define _BROWSER_CAMERA_

#if PLATFORM_GLES3

#include "Application/MiniGameCamera.h"
#include "Input/InputSystem.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
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

	Float32 dlen = sqrtf(dx*dx + dy*dy);
	Bool rotating = input.IsMouseDown(0) && (dlen > 0.0f);

	if (rotating)
	{
		glm::quat rot_y = glm::angleAxis(-dx * rotate_speed, glm::vec3(0, 1, 0));
		glm::quat rot_x = glm::angleAxis(-dy * rotate_speed, Right());
		orientation = glm::normalize(rot_y * orientation * rot_x);
	}

	// Scroll zoom
	if (scroll != 0.0f)
	{
		distance -= scroll * zoom_speed;
		if (distance < min_dist) distance = min_dist;
		if (distance > max_dist) distance = max_dist;
	}

	// Touch: single-finger drag = rotate
	CONST auto& touch = input.GetTouch();
	static Float32 tx_last = 0, ty_last = 0;
	static Bool touch_active = false;

	if (touch.pointer_count == 1 && touch.pointers[0].active)
	{
		if (!touch_active) { tx_last = touch.pointers[0].x; ty_last = touch.pointers[0].y; touch_active = true; }
		Float32 tdx = touch.pointers[0].x - tx_last;
		Float32 tdy = touch.pointers[0].y - ty_last;
		tx_last = touch.pointers[0].x; ty_last = touch.pointers[0].y;

		glm::quat rot_y = glm::angleAxis(-tdx * rotate_speed, glm::vec3(0, 1, 0));
		glm::quat rot_x = glm::angleAxis(-tdy * rotate_speed, Right());
		orientation = glm::normalize(rot_y * orientation * rot_x);
	}
	else { touch_active = false; }

	// Touch: two-finger pinch = zoom
	static Float32 last_pinch = 0;
	if (touch.pointer_count >= 2)
	{
		float d = sqrtf(powf(touch.pointers[0].x - touch.pointers[1].x, 2) +
		                powf(touch.pointers[0].y - touch.pointers[1].y, 2));
		if (last_pinch > 0) { distance *= (last_pinch / d); if (distance < min_dist) distance = min_dist; if (distance > max_dist) distance = max_dist; }
		last_pinch = d;
	}
	else { last_pinch = 0; }

	glm::vec3 eye = GetEyePosition();
	out_view.SetViewport(vp_w, vp_h);
	out_view.SetViewLookAt(eye, target, glm::vec3(0, 1, 0));
	out_view.UpdateMatrices();
}

inline void BrowserCamera::Reset()
{
	orientation = glm::quat{1, 0, 0, 0};
	distance = 6.0f;
	target = glm::vec3(0, 0, 0);
}

MYRENDERER_END_NAMESPACE  // Emscripten
MYRENDERER_END_NAMESPACE  // Platform
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _BROWSER_CAMERA_
