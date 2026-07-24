#pragma once
#ifndef _UIDATAMODELBINDER_
#define _UIDATAMODELBINDER_

#include "Core/ConstDefine.h"
#include <functional>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

/**
 * Abstract data model binder — eliminates void* and RmlUi types from
 * generated binding code and application callbacks.
 *
 * Each backend provides a concrete subclass (e.g. RmlDataModelBinder)
 * that translates these abstract calls into backend-specific API calls.
 *
 * Usage (from UIManager template):
 *   auto* binder = m_backend->GetModelBinder(model);
 *   if (binder) Traits::BindDataModel(binder, data);
 */
MYRENDERER_BEGIN_CLASS(UIDataModelBinder)
#pragma region METHOD
public:
	VIRTUAL ~UIDataModelBinder() MYDEFAULT;

	// ---- OneWay: C++ -> UI ----
	VIRTUAL void METHOD(Bind)(CONST String& name, Int* ptr)       { (void)name; (void)ptr; }
	VIRTUAL void METHOD(Bind)(CONST String& name, Float32* ptr)   { (void)name; (void)ptr; }
	VIRTUAL void METHOD(Bind)(CONST String& name, String* ptr)    { (void)name; (void)ptr; }
	VIRTUAL void METHOD(Bind)(CONST String& name, Bool* ptr)      { (void)name; (void)ptr; }

	// ---- TwoWay: C++ <-> UI ----
	VIRTUAL void METHOD(BindTwoWay)(CONST String& name,
		std::function<Int()> getter, std::function<void(Int)> setter)
	{ (void)name; (void)getter; (void)setter; }
	VIRTUAL void METHOD(BindTwoWay)(CONST String& name,
		std::function<Float32()> getter, std::function<void(Float32)> setter)
	{ (void)name; (void)getter; (void)setter; }
	VIRTUAL void METHOD(BindTwoWay)(CONST String& name,
		std::function<String()> getter, std::function<void(CONST String&)> setter)
	{ (void)name; (void)getter; (void)setter; }
	VIRTUAL void METHOD(BindTwoWay)(CONST String& name,
		std::function<Bool()> getter, std::function<void(Bool)> setter)
	{ (void)name; (void)getter; (void)setter; }

	// ---- Event: pure std::function<void()> ----
	VIRTUAL void METHOD(BindEvent)(CONST String& name, std::function<void()> callback)
	{ (void)name; (void)callback; }

protected:
private:
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UIDATAMODELBINDER_
