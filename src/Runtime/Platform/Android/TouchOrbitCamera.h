#pragma once
#ifndef _TOUCH_ORBIT_CAMERA_
#define _TOUCH_ORBIT_CAMERA_

#if PLATFORM_ANDROID

#include "Core/ConstDefine.h"
#include "Platform/PlatformWindow.h"
#include "Render/View/SceneView.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

// Touch-driven orbit camera for Android (quaternion rotation, no gimbal lock).
// Gesture mapping:
//   Single finger drag   = rotate (yaw world Y, pitch local X)
//   Pinch (two fingers)  = zoom  (exponential distance scaling)
//   Two finger drag      = pan target along view plane
MYRENDERER_BEGIN_CLASS(TouchOrbitCamera)
#pragma region METHOD
public:
	TouchOrbitCamera() MYDEFAULT;

	void METHOD(Update)(CONST TouchState& in_touch, Float32 in_dt,
	                    Render::SceneView& out_view);
	void METHOD(Reset)();

protected:
	Bool METHOD(IsDragStart)(CONST TouchState& touch) CONST;
	Bool METHOD(IsTwoFinger)(CONST TouchState& touch) CONST;
	glm::vec3 METHOD(Forward)() CONST { return orientation * glm::vec3(0, 0, 1); }
	glm::vec3 METHOD(Right)()   CONST { return orientation * glm::vec3(1, 0, 0); }
	glm::vec3 METHOD(Up)()      CONST { return orientation * glm::vec3(0, 1, 0); }

private:
#pragma endregion

#pragma region MEMBER
public:
	glm::quat orientation{ 1, 0, 0, 0 };
	Float32 distance{ 10.0f };
	glm::vec3 target{ 0.0f, 0.0f, 0.0f };
	Float32 rotate_speed{ 0.005f };
	Float32 zoom_step{ 0.9f };
	Float32 min_distance{ 0.5f };
	Float32 max_distance{ 500.0f };
	Bool enable_pan{ true };

protected:
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
