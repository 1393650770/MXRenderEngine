#pragma once
#ifndef _TOUCH_ORBIT_CAMERA_
#define _TOUCH_ORBIT_CAMERA_

#if PLATFORM_ANDROID

#include "Core/ConstDefine.h"
#include "Platform/PlatformWindow.h"
#include "Render/View/SceneView.h"
#include <glm/glm.hpp>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

// Touch-driven orbit camera for Android.
// Gesture mapping (same feel as OrbitCameraController on desktop):
//   Single finger drag   = rotate (yaw / pitch)
//   Pinch (two fingers)  = zoom  (exponential distance scaling)
//   Two finger drag      = pan target along view plane
//
// Usage: call Update(touch, dt, out_view) once per frame from your
// render loop. TouchState comes from AndroidApp::GetTouchState().
MYRENDERER_BEGIN_CLASS(TouchOrbitCamera)
#pragma region METHOD
public:
	TouchOrbitCamera() MYDEFAULT;

	// Process a frame of touch input and write the resulting camera
	// into out_view (SetViewport + SetViewLookAt + UpdateMatrices).
	void METHOD(Update)(CONST TouchState& in_touch, Float32 in_dt,
	                    Render::SceneView& out_view);

	// Reset the accumulated gesture state (call on first frame / resume).
	void METHOD(Reset)();

protected:
	// Detect a tap-to-rotate start (first finger down, no prior contact)
	Bool METHOD(IsDragStart)(CONST TouchState& touch) CONST;
	// Detect two-finger gesture active
	Bool METHOD(IsTwoFinger)(CONST TouchState& touch) CONST;

private:
#pragma endregion

#pragma region MEMBER
public:
	// Orbit parameters — same API as OrbitCameraController
	Float32 yaw{ 0.7f };
	Float32 pitch{ 0.25f };
	Float32 distance{ 10.0f };
	glm::vec3 target{ 0.0f, 0.0f, 0.0f };
	Float32 rotate_speed{ 0.005f };
	Float32 zoom_step{ 0.9f };
	Float32 min_distance{ 0.5f };
	Float32 max_distance{ 500.0f };
	Float32 min_pitch{ -1.45f };
	Float32 max_pitch{ 1.45f };
	Bool enable_pan{ true };

protected:
	// Previous frame touch state (for delta computation)
	Int     last_pointer_count = 0;
	Float32 last_x = 0.0f, last_y = 0.0f;
	Float32 last_pinch = 0.0f;
	Bool    was_dragging = false;
	Bool    was_two_finger = false;
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // PLATFORM_ANDROID
#endif // _TOUCH_ORBIT_CAMERA_
