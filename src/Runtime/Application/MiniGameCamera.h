#pragma once
#ifndef _MINIGAME_CAMERA_
#define _MINIGAME_CAMERA_

// MiniGame Camera Abstraction
// Patterned after UISystem: virtual with empty defaults.
// Rotation stored as quaternion — no gimbal lock, no axis drift.
// Platforms override Update() to poll their input (mouse/touch) and write
// the camera transform into a SceneView.

#include "Core/ConstDefine.h"
#include "Render/View/SceneView.h"
#include <glm/gtc/quaternion.hpp>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

MYRENDERER_BEGIN_CLASS(MiniGameCamera)
#pragma region METHOD
public:
	MiniGameCamera() MYDEFAULT;
	VIRTUAL ~MiniGameCamera() MYDEFAULT;

	// Process input and write camera state into out_view.
	VIRTUAL void METHOD(Update)(Float32 dt, UInt32 vp_w, UInt32 vp_h, Render::SceneView& out_view) {}

	// Reset orientation to identity
	VIRTUAL void METHOD(Reset)() { orientation = glm::quat{1,0,0,0}; }

	// Direction vectors extracted from orientation quaternion
	glm::vec3 METHOD(Forward)() CONST { return orientation * glm::vec3(0, 0, 1); }
	glm::vec3 METHOD(Right)()   CONST { return orientation * glm::vec3(1, 0, 0); }
	glm::vec3 METHOD(Up)()      CONST { return orientation * glm::vec3(0, 1, 0); }

	// Orbit eye position from orientation + distance
	glm::vec3 METHOD(GetEyePosition)() CONST { return target + Forward() * distance; }

#pragma region MEMBER
public:
	glm::quat orientation{ 1, 0, 0, 0 };  // rotation quaternion (identity = look at +Z)
	Float32   distance   = 6.0f;
	glm::vec3 target     { 0, 0, 0 };     // orbit center
	Float32 rotate_speed = 0.005f;
	Float32 zoom_speed   = 0.1f;
	Float32 min_dist     = 0.5f;
	Float32 max_dist     = 100.0f;
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Application
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _MINIGAME_CAMERA_
