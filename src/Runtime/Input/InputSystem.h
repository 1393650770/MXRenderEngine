#pragma once
#ifndef _INPUT_SYSTEM_
#define _INPUT_SYSTEM_

#include "Core/ConstDefine.h"
#include "Input/InputKeys.h"
#include "Platform/PlatformWindow.h" // TouchState

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Input)

// Per-frame input snapshot (replaces direct GLFW polling).
MYRENDERER_BEGIN_STRUCT(InputState)
public:
	static constexpr Int kMaxKeys = 512;
	static constexpr Int kMaxMouseButtons = 8;

	// Keyboard
	Bool keys[kMaxKeys] = {};
	Bool keys_prev[kMaxKeys] = {};

	// Mouse
	Float32 mouse_x = -1.0f, mouse_y = -1.0f;
	Float32 mouse_frame_start_x = -1.0f, mouse_frame_start_y = -1.0f;
	Float32 mouse_dx = 0.0f, mouse_dy = 0.0f;
	Bool mouse_buttons[kMaxMouseButtons] = {};
	Bool mouse_buttons_prev[kMaxMouseButtons] = {};
	Float32 scroll_delta = 0.0f;

	// Touch (mobile)
	TouchState touch;
MYRENDERER_END_STRUCT

// Singleton input accumulator. Per-frame: PlatformWindow feeds raw events,
// then game code queries processed state.
MYRENDERER_BEGIN_CLASS(InputSystem)
#pragma region METHOD
public:
	static InputSystem& METHOD(Get)();

	// Called by PlatformWindow at the START of each frame
	void METHOD(BeginFrame)();

	// Feed raw events (called by PlatformWindow during PollEvents)
	void METHOD(FeedKeyDown)(Int platform_code);
	void METHOD(FeedKeyUp)(Int platform_code);
	void METHOD(FeedMousePos)(Float32 x, Float32 y);
	void METHOD(FeedMouseButton)(Int btn, Bool down);
	void METHOD(FeedScroll)(Float32 delta);
	void METHOD(FeedTouch)(CONST TouchState& touch);

	// Query processed state (called by game code each frame)
	Bool METHOD(IsKeyDown)(CONST Key& key) CONST;
	Bool METHOD(IsKeyPressed)(CONST Key& key) CONST;
	Bool METHOD(IsKeyReleased)(CONST Key& key) CONST;
	Bool METHOD(IsMouseDown)(Int btn) CONST;
	Bool METHOD(IsMousePressed)(Int btn) CONST;
	Bool METHOD(IsMouseReleased)(Int btn) CONST;
	void METHOD(GetMousePos)(Float32& x, Float32& y) CONST;
	void METHOD(GetMouseDelta)(Float32& dx, Float32& dy) CONST;
	Float32 METHOD(GetScrollDelta)() CONST;
	CONST TouchState& METHOD(GetTouch)() CONST;

protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	InputState m_state;
	Bool m_first_frame = true;
private:
	static InputSystem s_instance;
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif
