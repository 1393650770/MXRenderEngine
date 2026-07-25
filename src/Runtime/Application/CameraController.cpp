#include "CameraController.h"
#include "Platform/PlatformWindow.h"
#include "Input/InputSystem.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

OrbitCameraController::~OrbitCameraController() MYDEFAULT;

void OrbitCameraController::Attach(PlatformWindow* in_window)
{
	window = in_window;
	if (window)
	{
		Float64 x, y;
		window->GetCursorPos(x, y);
		last_cursor_x = x;
		last_cursor_y = y;
	}
}

Float32 OrbitCameraController::ConsumeScrollDelta()
{
	return MXRender::Input::InputSystem::Get().GetScrollDelta();
}

void OrbitCameraController::Update(Float32 in_dt, Render::SceneView& out_view)
{
	if (window == nullptr)
		return;
	Int width = 0, height = 0;
	window->GetFramebufferSize(width, height);

	auto& input = MXRender::Input::InputSystem::Get();
	Float32 cx = 0, cy = 0;
	input.GetMousePos(cx, cy);
	Float32 delta_x = cx - (Float32)last_cursor_x;
	Float32 delta_y = cy - (Float32)last_cursor_y;

	// LMB drag → quaternion rotation (yaw around world Y, pitch around local X)
	Bool lmb_down = input.IsMouseDown((Int)MouseButton::Left);
	if (lmb_down && rotating)
	{
		glm::quat rot_y = glm::angleAxis(-delta_x * rotate_speed, glm::vec3(0, 1, 0));
		glm::quat rot_x = glm::angleAxis(-delta_y * rotate_speed, Right());
		orientation = glm::normalize(rot_y * orientation * rot_x);
	}
	rotating = lmb_down;

	// MMB drag → pan target in view plane (vectors from quaternion, no trig)
	Bool mmb_down = input.IsMouseDown((Int)MouseButton::Middle);
	if (enable_pan && mmb_down && panning && height > 0)
	{
		glm::vec3 forward = Forward();
		glm::vec3 right   = Right();
		glm::vec3 up      = Up();
		Float32 pan_scale = distance / (Float32)height;
		target += (-delta_x * right + delta_y * up) * pan_scale;
	}
	panning = mmb_down;

	last_cursor_x = (Float64)cx;
	last_cursor_y = (Float64)cy;

	// Scroll → exponential zoom
	Float32 scroll = ConsumeScrollDelta();
	if (scroll != 0.0f)
		distance = glm::clamp(distance * powf(zoom_step, scroll), min_distance, max_distance);

	if (width <= 0 || height <= 0)
		return;

	glm::vec3 eye = target + Forward() * distance;
	out_view.SetViewport((UInt32)width, (UInt32)height);
	out_view.SetViewLookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
	out_view.UpdateMatrices();
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
