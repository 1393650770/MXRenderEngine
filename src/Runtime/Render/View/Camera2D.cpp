#include "Camera2D.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)

// =========================================================================
// Ortho region
// =========================================================================

void Camera2D::SetOrthoRegion(Float32 left, Float32 right, Float32 bottom, Float32 top)
{
	CHECK_WITH_LOG(right <= left, "Camera2D: ortho right must be greater than left");
	CHECK_WITH_LOG(top <= bottom, "Camera2D: ortho top must be greater than bottom");
	ortho_left = left;
	ortho_right = right;
	ortho_bottom = bottom;
	ortho_top = top;
	ortho_size = top - bottom;
	auto_aspect = false;
}

void Camera2D::SetOrthoSize(Float32 world_height)
{
	CHECK_WITH_LOG(world_height <= 0.0f, "Camera2D: ortho size must be positive");
	Float32 aspect = GetAspectRatio();
	Float32 half_h = world_height * 0.5f;
	Float32 half_w = half_h * aspect;
	ortho_size = world_height;
	auto_aspect = true;
	ortho_left = -half_w;
	ortho_right = half_w;
	ortho_bottom = -half_h;
	ortho_top = half_h;
}

void Camera2D::SetOrthoScale(Float32 scale)
{
	CHECK_WITH_LOG(scale <= 0.0f, "Camera2D: ortho scale must be positive");
	ortho_scale = scale;
}

// =========================================================================
// Viewport
// =========================================================================

void Camera2D::SetViewport(UInt32 in_width, UInt32 in_height)
{
	CHECK_WITH_LOG(in_width == 0 || in_height == 0, "Camera2D: viewport dimensions must be non-zero");
	viewport_width = in_width;
	viewport_height = in_height;
	if (auto_aspect)
	{
		Float32 half_h = ortho_size * 0.5f;
		Float32 half_w = half_h * GetAspectRatio();
		ortho_left = -half_w;
		ortho_right = half_w;
		ortho_bottom = -half_h;
		ortho_top = half_h;
	}
}

// =========================================================================
// Transform
// =========================================================================

void Camera2D::SetPosition(CONST glm::vec2& in_position)
{
	position = in_position;
}

void Camera2D::SetZoom(Float32 in_zoom)
{
	CHECK_WITH_LOG(in_zoom <= 0.0f, "Camera2D: zoom must be positive");
	zoom = in_zoom;
}

void Camera2D::SetRotation(Float32 in_radians)
{
	rotation = in_radians;
}

void Camera2D::SetNearPlane(Float32 in_near)
{
	near_plane = in_near;
}

void Camera2D::SetFarPlane(Float32 in_far)
{
	far_plane = in_far;
}

Float32 Camera2D::GetAspectRatio() CONST
{
	if (viewport_height == 0) return 1.0f;
	return (Float32)viewport_width / (Float32)viewport_height;
}

// =========================================================================
// Matrix rebuild
// =========================================================================

void Camera2D::UpdateMatrices()
{
	CHECK_WITH_LOG(viewport_width == 0 || viewport_height == 0, "Camera2D: viewport dimensions must be non-zero");
	CHECK_WITH_LOG(far_plane <= near_plane, "Camera2D: far plane must be greater than near plane");
	RebuildView();
	RebuildProjection();
	view_projection_matrix = projection_matrix * view_matrix;
	inv_view_projection_matrix = glm::inverse(view_projection_matrix);
}

void Camera2D::RebuildProjection()
{
	// Apply zoom to the ortho region
	Float32 scale = ortho_scale / zoom;

	Float32 l = ortho_left * scale;
	Float32 r = ortho_right * scale;
	Float32 b = ortho_bottom * scale;
	Float32 t = ortho_top * scale;

	// glm::ortho with 0..1 depth range (GLM_FORCE_DEPTH_ZERO_TO_ONE must be defined)
	projection_matrix = glm::ortho(l, r, b, t, near_plane, far_plane);

	// Vulkan Y-flip: Y points down in clip space (same convention as SceneView)
	projection_matrix[1][1] *= -1.0f;
}

void Camera2D::RebuildView()
{
	// View matrix = translate(-position) * rotate(-rotation around Z)
	// This is the inverse of the world transform:
	//   world_to_view = inverse(translate(position) * rotate(rotation))
	//                 = rotate(-rotation) * translate(-position)
	glm::mat4 translation = glm::translate(glm::mat4(1.0f),
		glm::vec3(-position.x, -position.y, 0.0f));
	glm::mat4 rot = glm::rotate(glm::mat4(1.0f), -rotation,
		glm::vec3(0.0f, 0.0f, 1.0f));
	view_matrix = rot * translation;
}

// =========================================================================
// Coordinate conversion
// =========================================================================

glm::vec2 Camera2D::ScreenToWorld(Float32 screen_x, Float32 screen_y) CONST
{
	// Pixel to NDC (with Y-flip: screen Y goes top-to-bottom, NDC Y goes bottom-to-top)
	// The Vulkan Y-flip in the projection matrix means screen (0,0) = top-left
	// maps to NDC (0, 1) after the flip, so we DON'T double-flip here.
	Float32 ndc_x = 2.0f * screen_x / (Float32)viewport_width - 1.0f;
	Float32 ndc_y = 2.0f * screen_y / (Float32)viewport_height - 1.0f;

	// Inverse view-projection (depth = 0, near plane)
	glm::vec4 world_h = inv_view_projection_matrix * glm::vec4(ndc_x, ndc_y, 0.0f, 1.0f);
	if (glm::abs(world_h.w) < 0.0001f)
		return glm::vec2(0.0f, 0.0f);
	return glm::vec2(world_h.x / world_h.w, world_h.y / world_h.w);
}

glm::vec2 Camera2D::WorldToScreen(Float32 world_x, Float32 world_y) CONST
{
	// World to clip space
	glm::vec4 clip = view_projection_matrix * glm::vec4(world_x, world_y, 0.0f, 1.0f);
	if (glm::abs(clip.w) < 0.0001f)
		return glm::vec2(0.0f, 0.0f);

	// Perspective divide
	glm::vec2 ndc = glm::vec2(clip.x / clip.w, clip.y / clip.w);

	// NDC to pixel
	Float32 screen_x = (ndc.x + 1.0f) * 0.5f * (Float32)viewport_width;
	Float32 screen_y = (ndc.y + 1.0f) * 0.5f * (Float32)viewport_height;
	return glm::vec2(screen_x, screen_y);
}

// =========================================================================
// Frustum query
// =========================================================================

glm::vec2 Camera2D::GetFrustumTopLeft() CONST
{
	return ScreenToWorld(0.0f, 0.0f);
}

glm::vec2 Camera2D::GetFrustumBottomRight() CONST
{
	return ScreenToWorld((Float32)viewport_width, (Float32)viewport_height);
}

void Camera2D::GetVisibleBounds(glm::vec2& out_min, glm::vec2& out_max) CONST
{
	glm::vec2 corners[] = {
		ScreenToWorld(0.0f, 0.0f),
		ScreenToWorld((Float32)viewport_width, 0.0f),
		ScreenToWorld(0.0f, (Float32)viewport_height),
		ScreenToWorld((Float32)viewport_width, (Float32)viewport_height)
	};
	out_min = corners[0];
	out_max = corners[0];
	for (UInt32 i = 1; i < 4; ++i)
	{
		if (corners[i].x < out_min.x) out_min.x = corners[i].x;
		if (corners[i].y < out_min.y) out_min.y = corners[i].y;
		if (corners[i].x > out_max.x) out_max.x = corners[i].x;
		if (corners[i].y > out_max.y) out_max.y = corners[i].y;
	}
}

MYRENDERER_END_NAMESPACE  // Render
MYRENDERER_END_NAMESPACE  // MXRender
