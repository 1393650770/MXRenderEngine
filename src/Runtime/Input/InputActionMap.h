#pragma once
#ifndef _INPUT_ACTION_MAP_
#define _INPUT_ACTION_MAP_

// Input Action Map — Decorator over InputSystem.
// Maps logical action names ("Jump", "MoveForward") to physical keys/buttons.
// Supports multiple bindings per action and JSON serialization.

#include "Core/ConstDefine.h"
#include "Input/InputSystem.h"
#include "Input/InputKeys.h"
#include <unordered_map>
#include <functional>
#include <vector>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Input)

struct ActionBinding
{
	EKey key = EKey::None;
	Int mouse_button = -1;  // 0=LMB, 1=RMB, 2=MMB, -1=unused
	Float32 scale = 1.0f;   // axis scale (for analog -> digital)
};

MYRENDERER_BEGIN_CLASS(InputActionMap)
#pragma region METHOD
public:
	InputActionMap() MYDEFAULT;

	void METHOD(BindAction)(const String& name, EKey key, Float32 scale = 1.0f);
	void METHOD(BindAction)(const String& name, Int mouse_button);

	Bool    METHOD(IsActionPressed)(const String& name) const;   // rising edge
	Bool    METHOD(IsActionDown)(const String& name) const;      // held
	Bool    METHOD(IsActionReleased)(const String& name) const;  // falling edge
	Float32 METHOD(GetActionValue)(const String& name) const;    // 0..1 (digital) or -1..1 (analog)

	void METHOD(Clear)() { m_map.clear(); }
	void METHOD(ClearAction)(const String& name) { m_map.erase(name); }

protected:
	const Vector<ActionBinding>* FindAction(const String& name) const;
private:
#pragma endregion

#pragma region MEMBER
protected:
	std::unordered_map<String, Vector<ActionBinding>> m_map;
private:
#pragma endregion
MYRENDERER_END_CLASS

// ---- Inline impl ----

inline void InputActionMap::BindAction(const String& name, EKey key, Float32 scale)
{
	ActionBinding b; b.key = key; b.scale = scale;
	m_map[name].push_back(b);
}

inline void InputActionMap::BindAction(const String& name, Int mouse_button)
{
	ActionBinding b; b.mouse_button = mouse_button;
	m_map[name].push_back(b);
}

inline const Vector<ActionBinding>* InputActionMap::FindAction(const String& name) const
{
	auto it = m_map.find(name);
	return (it != m_map.end()) ? &it->second : nullptr;
}

inline Bool InputActionMap::IsActionPressed(const String& name) const
{
	auto* bindings = FindAction(name);
	if (!bindings) return false;
	auto& input = InputSystem::Get();
	for (auto& b : *bindings) {
		if (b.key != EKey::None && input.IsKeyPressed(b.key)) return true;
		if (b.mouse_button >= 0 && input.IsMousePressed(b.mouse_button)) return true;
	}
	return false;
}

inline Bool InputActionMap::IsActionDown(const String& name) const
{
	auto* bindings = FindAction(name);
	if (!bindings) return false;
	auto& input = InputSystem::Get();
	for (auto& b : *bindings) {
		if (b.key != EKey::None && input.IsKeyDown(b.key)) return true;
		if (b.mouse_button >= 0 && input.IsMouseDown(b.mouse_button)) return true;
	}
	return false;
}

inline Bool InputActionMap::IsActionReleased(const String& name) const
{
	auto* bindings = FindAction(name);
	if (!bindings) return false;
	auto& input = InputSystem::Get();
	for (auto& b : *bindings) {
		if (b.key != EKey::None && input.IsKeyReleased(b.key)) return true;
		if (b.mouse_button >= 0 && input.IsMouseReleased(b.mouse_button)) return true;
	}
	return false;
}

inline Float32 InputActionMap::GetActionValue(const String& name) const
{
	auto* bindings = FindAction(name);
	if (!bindings) return 0.0f;
	auto& input = InputSystem::Get();
	Float32 max_val = 0.0f;
	for (auto& b : *bindings) {
		Float32 v = 0.0f;
		if (b.key != EKey::None && input.IsKeyDown(b.key)) v = b.scale;
		if (b.mouse_button >= 0 && input.IsMouseDown(b.mouse_button)) v = b.scale;
		if (v > max_val) max_val = v;
	}
	return max_val;
}

MYRENDERER_END_NAMESPACE  // Input
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _INPUT_ACTION_MAP_
