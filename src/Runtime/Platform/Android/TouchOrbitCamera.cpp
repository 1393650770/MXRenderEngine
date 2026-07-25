#include "TouchOrbitCamera.h"

#if PLATFORM_ANDROID

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

Bool TouchOrbitCamera::IsDragStart(CONST TouchState& touch) CONST
{
	return touch.pointer_count == 1
		&& touch.pointers[0].active
		&& !was_dragging
		&& !was_two_finger;
}

Bool TouchOrbitCamera::IsTwoFinger(CONST TouchState& touch) CONST
{
	return touch.pointer_count >= 2
		&& touch.pointers[0].active
		&& touch.pointers[1].active;
}

void TouchOrbitCamera::Reset()
{
	last_pointer_count = 0;
	last_x = last_y = 0.0f;
	last_pinch = 0.0f;
	was_dragging = false;
	was_two_finger = false;
}

void TouchOrbitCamera::Update(CONST TouchState& in_touch, Float32 in_dt,
                              Render::SceneView& out_view)
{
	Int width = out_view.GetViewportWidth();
	Int height = out_view.GetViewportHeight();
	if (width <= 0 || height <= 0)
		return;

	Bool two_finger = IsTwoFinger(in_touch);

	if (in_touch.pointer_count == 0)
	{
		// No fingers on screen — reset tracking
		was_dragging = false;
		was_two_finger = false;
		last_pointer_count = 0;
		return;
	}

	if (two_finger)
	{
		// Two-finger gestures: pinch = zoom, drag = pan
		Float32 current_pinch = in_touch.pinch_distance;

		if (was_two_finger && last_pinch > 0.0f && current_pinch > 0.0f)
		{
			// Pinch zoom
			Float32 ratio = current_pinch / last_pinch;
			if (ratio < 0.98f || ratio > 1.02f)
			{
				distance = glm::clamp(distance / ratio, min_distance, max_distance);
			}
			else if (enable_pan)
			{
				// Two-finger pan: compute midpoint delta
				Float32 mid_x = (in_touch.pointers[0].x + in_touch.pointers[1].x) * 0.5f;
				Float32 mid_y = (in_touch.pointers[0].y + in_touch.pointers[1].y) * 0.5f;
				Float32 delta_x = mid_x - last_x;
				Float32 delta_y = mid_y - last_y;

				if (fabsf(delta_x) > 0.1f || fabsf(delta_y) > 0.1f)
				{
					glm::vec3 forward = -glm::vec3(cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw));
					glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
					glm::vec3 up = glm::normalize(glm::cross(right, forward));
					Float32 pan_scale = distance / (Float32)height;
					target += (-delta_x * right + delta_y * up) * pan_scale;
				}

				last_x = mid_x;
				last_y = mid_y;
			}
		}

		last_pinch = current_pinch;
		was_two_finger = true;
		was_dragging = false;
	}
	else if (in_touch.pointer_count == 1)
	{
		Float32 cx = in_touch.pointers[0].x;
		Float32 cy = in_touch.pointers[0].y;

		if (was_dragging && (was_two_finger == false))
		{
			Float32 delta_x = cx - last_x;
			Float32 delta_y = cy - last_y;
			if (fabsf(delta_x) > 0.1f || fabsf(delta_y) > 0.1f)
			{
				yaw += delta_x * rotate_speed;
				pitch = glm::clamp(pitch + delta_y * rotate_speed, min_pitch, max_pitch);
			}
		}

		last_x = cx;
		last_y = cy;
		was_dragging = true;
		was_two_finger = false;
	}

	last_pointer_count = in_touch.pointer_count;

	// Compute final camera
	glm::vec3 eye = target + distance * glm::vec3(
		cosf(pitch) * sinf(yaw),
		sinf(pitch),
		cosf(pitch) * cosf(yaw));
	out_view.SetViewport((UInt32)width, (UInt32)height);
	out_view.SetViewLookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
	out_view.UpdateMatrices();
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // PLATFORM_ANDROID
