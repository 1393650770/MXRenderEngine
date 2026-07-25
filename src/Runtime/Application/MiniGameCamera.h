#pragma once
#ifndef _MINIGAME_CAMERA_
#define _MINIGAME_CAMERA_

// MiniGame Camera Abstraction
// Patterned after UISystem: virtual with empty defaults.
// Platforms override Update() to poll their input (mouse/touch) and write
// the camera transform into a SceneView.

#include "Core/ConstDefine.h"
#include "Render/View/SceneView.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

MYRENDERER_BEGIN_CLASS(MiniGameCamera)
#pragma region METHOD
public:
	MiniGameCamera() MYDEFAULT;
	VIRTUAL ~MiniGameCamera() MYDEFAULT;

	// Process input and write camera state into out_view.
	// Platforms override to read from InputSystem / wx.onTouch / tt.onTouch.
	VIRTUAL void METHOD(Update)(Float32 dt, UInt32 vp_w, UInt32 vp_h, Render::SceneView& out_view) {}

	// Reset accumulated gesture state
	VIRTUAL void METHOD(Reset)() {}

#pragma region MEMBER
public:
	Float32 yaw        = 0.7f;
	Float32 pitch      = 0.25f;
	Float32 distance   = 6.0f;
	glm::vec3 target   { 0, 0, 0 };
	Float32 rotate_speed = 0.005f;
	Float32 zoom_speed   = 0.1f;
	Float32 min_dist   = 0.5f;
	Float32 max_dist   = 100.0f;
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Application
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _MINIGAME_CAMERA_
