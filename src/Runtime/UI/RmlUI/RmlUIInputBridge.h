#pragma once
#ifndef _RMLUIINPUTBRIDGE_
#define _RMLUIINPUTBRIDGE_

#include "Core/ConstDefine.h"
#include "Input/InputKeys.h"
#include "UI/UIInputBridge.h"  // abstract base

namespace Rml {
class Context;
}

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(RmlUI)

/**
 * RmlUI-specific input bridge — implements the UIInputBridge interface.
 *
 * Routes platform InputSystem events to Rml::Context.  Application code
 * accesses this through the UIInputBridge* returned by UIManager and
 * never includes this header directly.
 */
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RmlUIInputBridge, public UIInputBridge)

#pragma region METHOD
public:
	RmlUIInputBridge() MYDEFAULT;
	VIRTUAL ~RmlUIInputBridge() MYDEFAULT;

	// --- UIInputBridge interface ---
	VIRTUAL void METHOD(ProcessMouseMove)(Int x, Int y) OVERRIDE FINAL;
	VIRTUAL void METHOD(ProcessMouseButton)(Int button, bool pressed) OVERRIDE FINAL;
	VIRTUAL void METHOD(ProcessMouseScroll)(Float32 dx, Float32 dy) OVERRIDE FINAL;
	VIRTUAL void METHOD(ProcessMouseLeave)() OVERRIDE FINAL;
	VIRTUAL void METHOD(ProcessKey)(CONST MXRender::Input::Key& key, bool pressed) OVERRIDE FINAL;
	VIRTUAL void METHOD(ProcessChar)(UInt32 codepoint) OVERRIDE FINAL;
	VIRTUAL bool METHOD(IsMouseInteracting)() CONST OVERRIDE FINAL;

	// --- Backend-specific ---
	/// Set the RmlUI context.  Called by RmlUISystem during Init.
	/// Not part of the UIInputBridge interface — RmlUI-internal only.
	void METHOD(SetContext)(Rml::Context* context);

protected:
private:
	Int  METHOD(BuildModifiers)() CONST;
	static Int METHOD(MapKey)(CONST MXRender::Input::Key& key);
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
	Rml::Context* m_context = nullptr;
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE // RmlUI
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_RMLUIINPUTBRIDGE_
