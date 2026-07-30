#pragma once
#ifndef _CAMERA_CONTROLLER_2D_
#define _CAMERA_CONTROLLER_2D_

#include "Core/ConstDefine.h"
#include "Platform/PlatformWindow.h"
#include "Render/View/Camera2D.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

// Interactive controller for Camera2D.
// Input mapping:
//   MMB drag - pan while preserving the grabbed world point
//   scroll   - zoom around the cursor
// Camera math remains in Camera2D; this class only translates input into state.
MYRENDERER_BEGIN_CLASS(CameraController2D)
#pragma region METHOD
public:
	CameraController2D() MYDEFAULT;
	~CameraController2D() MYDEFAULT;

	void METHOD(Attach)(PlatformWindow* in_window);
	void METHOD(Detach)();
	void METHOD(Update)(Float32 in_dt, Render::Camera2D& out_camera);

	void METHOD(SetFollowTarget)(CONST glm::vec2& in_target, Float32 in_speed = 5.0f);
	void METHOD(UpdateFollowTarget)(CONST glm::vec2& in_target);
	void METHOD(ClearFollowTarget)();

	// Restricts the visible camera area to these world-space bounds.
	void METHOD(SetBounds)(CONST glm::vec2& in_min, CONST glm::vec2& in_max);
	void METHOD(ClearBounds)();

protected:
	void METHOD(ApplyPan)(Render::Camera2D& camera, Float32 cursor_x, Float32 cursor_y,
		Float32 delta_x, Float32 delta_y);
	void METHOD(ApplyZoom)(Render::Camera2D& camera, Float32 cursor_x, Float32 cursor_y,
		Float32 scroll_delta);
	void METHOD(ApplyFollow)(Float32 dt, Render::Camera2D& camera);
	void METHOD(ApplyBounds)(Render::Camera2D& camera);

private:
#pragma endregion

#pragma region MEMBER
public:
	Float32 zoom_step{ 0.9f };
	Float32 min_zoom{ 0.01f };
	Float32 max_zoom{ 100.0f };
	Float32 follow_speed{ 5.0f };
	MouseButton pan_button{ MouseButton::Middle };
	Bool enable_pan{ true };
	Bool enable_zoom{ true };

protected:
	PlatformWindow* window{ nullptr };
	Bool has_follow_target{ false };
	glm::vec2 follow_target{ 0.0f, 0.0f };
	Bool has_bounds{ false };
	glm::vec2 bounds_min{ 0.0f, 0.0f };
	glm::vec2 bounds_max{ 0.0f, 0.0f };

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Application
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _CAMERA_CONTROLLER_2D_
