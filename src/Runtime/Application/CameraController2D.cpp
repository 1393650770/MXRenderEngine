#include "Application/CameraController2D.h"
#include "Input/InputSystem.h"
#include <cmath>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

void CameraController2D::Attach(PlatformWindow* in_window)
{
	window = in_window;
}

void CameraController2D::Detach()
{
	window = nullptr;
}

void CameraController2D::Update(Float32 in_dt, Render::Camera2D& out_camera)
{
	if (window == nullptr)
		return;

	Int width = 0;
	Int height = 0;
	window->GetFramebufferSize(width, height);
	if (width <= 0 || height <= 0)
		return;

	if (out_camera.GetViewportWidth() != (UInt32)width ||
		out_camera.GetViewportHeight() != (UInt32)height)
	{
		out_camera.SetViewport((UInt32)width, (UInt32)height);
		out_camera.UpdateMatrices();
	}

	auto& input = MXRender::Input::InputSystem::Get();
	Float32 cursor_x = 0.0f;
	Float32 cursor_y = 0.0f;
	Float32 delta_x = 0.0f;
	Float32 delta_y = 0.0f;
	input.GetMousePos(cursor_x, cursor_y);
	input.GetMouseDelta(delta_x, delta_y);

	if (enable_pan && input.IsMouseDown((Int)pan_button))
		ApplyPan(out_camera, cursor_x, cursor_y, delta_x, delta_y);

	Float32 scroll = input.GetScrollDelta();
	if (enable_zoom && scroll != 0.0f)
		ApplyZoom(out_camera, cursor_x, cursor_y, scroll);

	ApplyFollow(in_dt, out_camera);
	ApplyBounds(out_camera);
	out_camera.UpdateMatrices();
}

void CameraController2D::SetFollowTarget(CONST glm::vec2& in_target, Float32 in_speed)
{
	CHECK_WITH_LOG(in_speed < 0.0f, "CameraController2D: follow speed cannot be negative");
	has_follow_target = true;
	follow_target = in_target;
	follow_speed = in_speed;
}

void CameraController2D::UpdateFollowTarget(CONST glm::vec2& in_target)
{
	follow_target = in_target;
}

void CameraController2D::ClearFollowTarget()
{
	has_follow_target = false;
}

void CameraController2D::SetBounds(CONST glm::vec2& in_min, CONST glm::vec2& in_max)
{
	CHECK_WITH_LOG(in_max.x <= in_min.x || in_max.y <= in_min.y,
		"CameraController2D: bounds max must be greater than min");
	has_bounds = true;
	bounds_min = in_min;
	bounds_max = in_max;
}

void CameraController2D::ClearBounds()
{
	has_bounds = false;
}

void CameraController2D::ApplyPan(Render::Camera2D& camera, Float32 cursor_x,
	Float32 cursor_y, Float32 delta_x, Float32 delta_y)
{
	if (delta_x == 0.0f && delta_y == 0.0f)
		return;

	// Keep the world point from the previous cursor position under the cursor.
	glm::vec2 previous_world = camera.ScreenToWorld(cursor_x - delta_x, cursor_y - delta_y);
	glm::vec2 current_world = camera.ScreenToWorld(cursor_x, cursor_y);
	camera.SetPosition(camera.GetPosition() + previous_world - current_world);
	camera.UpdateMatrices();
}

void CameraController2D::ApplyZoom(Render::Camera2D& camera, Float32 cursor_x,
	Float32 cursor_y, Float32 scroll_delta)
{
	CHECK_WITH_LOG(zoom_step <= 0.0f || zoom_step >= 1.0f,
		"CameraController2D: zoom_step must be in the open interval (0, 1)");
	CHECK_WITH_LOG(min_zoom <= 0.0f || max_zoom < min_zoom,
		"CameraController2D: invalid zoom range");

	glm::vec2 world_before = camera.ScreenToWorld(cursor_x, cursor_y);
	Float32 next_zoom = glm::clamp(
		camera.GetZoom() * std::pow(zoom_step, -scroll_delta),
		min_zoom, max_zoom);

	camera.SetZoom(next_zoom);
	camera.UpdateMatrices();

	// Compensate position so zooming is anchored at the cursor.
	glm::vec2 world_after = camera.ScreenToWorld(cursor_x, cursor_y);
	camera.SetPosition(camera.GetPosition() + world_before - world_after);
	camera.UpdateMatrices();
}

void CameraController2D::ApplyFollow(Float32 dt, Render::Camera2D& camera)
{
	if (!has_follow_target || follow_speed <= 0.0f)
		return;

	Float32 alpha = 1.0f - std::exp(-follow_speed * dt);
	camera.SetPosition(glm::mix(camera.GetPosition(), follow_target, glm::clamp(alpha, 0.0f, 1.0f)));
	camera.UpdateMatrices();
}

void CameraController2D::ApplyBounds(Render::Camera2D& camera)
{
	if (!has_bounds)
		return;

	glm::vec2 visible_min;
	glm::vec2 visible_max;
	camera.GetVisibleBounds(visible_min, visible_max);
	glm::vec2 visible_size = visible_max - visible_min;
	glm::vec2 bounds_size = bounds_max - bounds_min;
	glm::vec2 correction{ 0.0f, 0.0f };

	if (visible_size.x >= bounds_size.x)
		correction.x = (bounds_min.x + bounds_max.x) * 0.5f - camera.GetPosition().x;
	else if (visible_min.x < bounds_min.x)
		correction.x = bounds_min.x - visible_min.x;
	else if (visible_max.x > bounds_max.x)
		correction.x = bounds_max.x - visible_max.x;

	if (visible_size.y >= bounds_size.y)
		correction.y = (bounds_min.y + bounds_max.y) * 0.5f - camera.GetPosition().y;
	else if (visible_min.y < bounds_min.y)
		correction.y = bounds_min.y - visible_min.y;
	else if (visible_max.y > bounds_max.y)
		correction.y = bounds_max.y - visible_max.y;

	if (correction.x != 0.0f || correction.y != 0.0f)
	{
		camera.SetPosition(camera.GetPosition() + correction);
		camera.UpdateMatrices();
	}
}

MYRENDERER_END_NAMESPACE  // Application
MYRENDERER_END_NAMESPACE  // MXRender
