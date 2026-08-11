#include "InputSystem.h"
#include <cstring>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Input)

InputSystem InputSystem::s_instance;

InputSystem& InputSystem::Get()
{
	return s_instance;
}

void InputSystem::BeginFrame()
{
	std::memcpy(m_state.keys_prev, m_state.keys, sizeof(m_state.keys));
	m_state.mouse_dx = 0.0f;
	m_state.mouse_dy = 0.0f;
	m_state.scroll_delta = 0.0f;
	m_chars.clear();
	std::memcpy(m_state.mouse_buttons_prev, m_state.mouse_buttons, sizeof(m_state.mouse_buttons));
	m_state.touch.pointer_count = 0;

	// Save current mouse position as frame-start reference.
	// GetMouseDelta returns displacement from this reference, so
	// sub-pixel jitter that oscillates cancels out (net delta ~= 0).
	m_state.mouse_frame_start_x = m_state.mouse_x;
	m_state.mouse_frame_start_y = m_state.mouse_y;

	if (m_first_frame)
	{
		m_first_frame = false;
		std::memset(m_state.keys_prev, 0, sizeof(m_state.keys_prev));
		std::memset(m_state.mouse_buttons_prev, 0, sizeof(m_state.mouse_buttons_prev));
	}
}

void InputSystem::FeedKeyDown(Int code)
{
	if (code >= 0 && code < InputState::kMaxKeys)
		m_state.keys[code] = true;
}

void InputSystem::FeedKeyUp(Int code)
{
	if (code >= 0 && code < InputState::kMaxKeys)
		m_state.keys[code] = false;
}

void InputSystem::FeedMousePos(Float32 x, Float32 y)
{
	// First mouse event: just store position, no delta (avoids 0→cursor jump)
	if (m_state.mouse_x < 0.0f)
	{
		m_state.mouse_x = x;
		m_state.mouse_y = y;
		return;
	}
	m_state.mouse_dx += x - m_state.mouse_x;
	m_state.mouse_dy += y - m_state.mouse_y;
	m_state.mouse_x = x;
	m_state.mouse_y = y;
}

void InputSystem::FeedMouseButton(Int btn, Bool down)
{
	if (btn >= 0 && btn < InputState::kMaxMouseButtons)
		m_state.mouse_buttons[btn] = down;
}

void InputSystem::FeedScroll(Float32 delta)
{
	m_state.scroll_delta += delta;
}

void InputSystem::FeedChar(UInt32 codepoint)
{
	m_chars.push_back(codepoint);
}

void InputSystem::FeedTouch(CONST TouchState& touch)
{
	m_state.touch = touch;
}

Bool InputSystem::IsKeyDown(CONST Key& key) CONST
{
	Int code = PlatformKeyMap::ToCode(key);
	return (code >= 0 && code < InputState::kMaxKeys) ? m_state.keys[code] : false;
}

Bool InputSystem::IsKeyPressed(CONST Key& key) CONST
{
	Int code = PlatformKeyMap::ToCode(key);
	if (code < 0 || code >= InputState::kMaxKeys) return false;
	return m_state.keys[code] && !m_state.keys_prev[code];
}

Bool InputSystem::IsKeyReleased(CONST Key& key) CONST
{
	Int code = PlatformKeyMap::ToCode(key);
	if (code < 0 || code >= InputState::kMaxKeys) return false;
	return !m_state.keys[code] && m_state.keys_prev[code];
}

Bool InputSystem::IsMouseDown(Int btn) CONST
{
	return (btn >= 0 && btn < InputState::kMaxMouseButtons) ? m_state.mouse_buttons[btn] : false;
}

Bool InputSystem::IsMousePressed(Int btn) CONST
{
	if (btn < 0 || btn >= InputState::kMaxMouseButtons) return false;
	return m_state.mouse_buttons[btn] && !m_state.mouse_buttons_prev[btn];
}

Bool InputSystem::IsMouseReleased(Int btn) CONST
{
	if (btn < 0 || btn >= InputState::kMaxMouseButtons) return false;
	return !m_state.mouse_buttons[btn] && m_state.mouse_buttons_prev[btn];
}

void InputSystem::GetMousePos(Float32& x, Float32& y) CONST
{
	x = m_state.mouse_x;
	y = m_state.mouse_y;
}

void InputSystem::GetMouseDelta(Float32& dx, Float32& dy) CONST
{
	// Delta from frame-start position (not accumulated intermediate deltas).
	// This cancels out sub-pixel jitter: if the mouse oscillates around
	// a point, the net displacement from frame_start is near zero.
	if (m_state.mouse_frame_start_x >= 0.0f)
	{
		dx = m_state.mouse_x - m_state.mouse_frame_start_x;
		dy = m_state.mouse_y - m_state.mouse_frame_start_y;
	}
	else
	{
		dx = m_state.mouse_dx;
		dy = m_state.mouse_dy;
	}
}

Float32 InputSystem::GetScrollDelta() CONST { return m_state.scroll_delta; }

CONST TouchState& InputSystem::GetTouch() CONST { return m_state.touch; }

CONST Vector<UInt32>& InputSystem::GetCharsThisFrame() CONST { return m_chars; }

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
