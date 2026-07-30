#pragma once
#ifndef _CAMERA_2D_
#define _CAMERA_2D_

// 2D orthographic camera -- pure math container, zero RHI dependency.
// Follows the same pattern as SceneView (Render/View/SceneView.h).
//
// Vulkan conventions (same as SceneView):
//   depth range 0..1 (GLM_FORCE_DEPTH_ZERO_TO_ONE)
//   Y-flip baked into projection (proj[1][1] *= -1)
//   world +Y appears upward while screen pixels use a top-left origin
//
// Usage:
//   Camera2D cam;
//   cam.SetViewport(1280, 960);
//   cam.SetOrthoSize(10.0f);        // 10 world units tall
//   cam.SetPosition(glm::vec2(0,0));
//   cam.UpdateMatrices();
//   glm::mat4 vp = cam.GetViewProjectionMatrix();
//   glm::vec2 world = cam.ScreenToWorld(mouse_x, mouse_y);

#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)

MYRENDERER_BEGIN_CLASS(Camera2D)
#pragma region METHOD
public:
	Camera2D() MYDEFAULT;
	~Camera2D() MYDEFAULT;

	// ---- Orthographic region ----
	// Define the visible world rectangle directly.
	void METHOD(SetOrthoRegion)(Float32 left, Float32 right, Float32 bottom, Float32 top);
	// Define visible area by world-space height (width auto-computed from aspect).
	void METHOD(SetOrthoSize)(Float32 world_height);
	// Additional scale factor applied on top of region/size (for zoom).
	void METHOD(SetOrthoScale)(Float32 scale);

	// ---- Viewport (screen pixel dimensions) ----
	void METHOD(SetViewport)(UInt32 in_width, UInt32 in_height);

	// ---- Transform ----
	void METHOD(SetPosition)(CONST glm::vec2& in_position);
	void METHOD(SetZoom)(Float32 in_zoom);
	void METHOD(SetRotation)(Float32 in_radians);

	// ---- Depth range ----
	void METHOD(SetNearPlane)(Float32 in_near);
	void METHOD(SetFarPlane)(Float32 in_far);

	// ---- Rebuild matrices from current parameters ----
	// Projection = orthographic (0..1 depth) with Vulkan Y-flip.
	// View = translation + rotation + scale from position/zoom/rotation.
	void METHOD(UpdateMatrices)();

	// ---- Matrix accessors ----
	CONST glm::mat4& METHOD(GetViewMatrix)() CONST { return view_matrix; }
	CONST glm::mat4& METHOD(GetProjectionMatrix)() CONST { return projection_matrix; }
	CONST glm::mat4& METHOD(GetViewProjectionMatrix)() CONST { return view_projection_matrix; }
	CONST glm::mat4& METHOD(GetInvViewProjectionMatrix)() CONST { return inv_view_projection_matrix; }

	// ---- Coordinate conversion ----
	// Screen pixels to world space (2D). Assumes 0..1 depth, Y-flip.
	glm::vec2 METHOD(ScreenToWorld)(Float32 screen_x, Float32 screen_y) CONST;
	// World space to screen pixels.
	glm::vec2 METHOD(WorldToScreen)(Float32 world_x, Float32 world_y) CONST;

	// ---- Getters ----
	CONST glm::vec2& METHOD(GetPosition)() CONST { return position; }
	Float32 METHOD(GetZoom)() CONST { return zoom; }
	Float32 METHOD(GetRotation)() CONST { return rotation; }
	UInt32 METHOD(GetViewportWidth)() CONST { return viewport_width; }
	UInt32 METHOD(GetViewportHeight)() CONST { return viewport_height; }
	Float32 METHOD(GetAspectRatio)() CONST;
	Float32 METHOD(GetOrthoScale)() CONST { return ortho_scale; }
	Float32 METHOD(GetOrthoLeft)() CONST { return ortho_left; }
	Float32 METHOD(GetOrthoRight)() CONST { return ortho_right; }
	Float32 METHOD(GetOrthoBottom)() CONST { return ortho_bottom; }
	Float32 METHOD(GetOrthoTop)() CONST { return ortho_top; }

	// ---- Frustum query (world-space corners and AABB, for culling) ----
	glm::vec2 METHOD(GetFrustumTopLeft)() CONST;
	glm::vec2 METHOD(GetFrustumBottomRight)() CONST;
	void METHOD(GetVisibleBounds)(glm::vec2& out_min, glm::vec2& out_max) CONST;

protected:
	void METHOD(RebuildProjection)();
	void METHOD(RebuildView)();

private:
#pragma endregion

#pragma region MEMBER
public:
protected:
	// Ortho region (world space, before zoom)
	Float32 ortho_left{ -10.0f };
	Float32 ortho_right{ 10.0f };
	Float32 ortho_bottom{ -10.0f };
	Float32 ortho_top{ 10.0f };
	Float32 ortho_size{ 20.0f };
	Float32 ortho_scale{ 1.0f };
	Bool auto_aspect{ false };

	// Transform
	glm::vec2 position{ 0.0f, 0.0f };
	Float32 zoom{ 1.0f };
	Float32 rotation{ 0.0f };  // radians around Z

	// Depth
	Float32 near_plane{ -100.0f };
	Float32 far_plane{ 100.0f };

	// Viewport
	UInt32 viewport_width{ 1280 };
	UInt32 viewport_height{ 960 };

	// Matrices
	glm::mat4 view_matrix{ 1.0f };
	glm::mat4 projection_matrix{ 1.0f };
	glm::mat4 view_projection_matrix{ 1.0f };
	glm::mat4 inv_view_projection_matrix{ 1.0f };

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Render
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _CAMERA_2D_
