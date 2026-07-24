/**
 * @deprecated Use UISystem instead.  UIBase is retained only for
 * EditorUI's AttachUiLayer path and will be removed once EditorUI
 * migrates to UISystem (Step 8).
 */

#pragma once
#ifndef _UIBASE_
#define _UIBASE_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

// Forward declarations
class UIRenderer;

MYRENDERER_BEGIN_CLASS(UIBase)

#pragma region METHOD
public:
	VIRTUAL ~UIBase() MYDEFAULT;
	UIBase() MYDEFAULT;

	VIRTUAL void METHOD(Init)() {};
	VIRTUAL void METHOD(Update)() {};
	VIRTUAL void METHOD(Draw)() {};
	VIRTUAL void METHOD(Release)() {};

	/// Returns the renderer backend for this UI system.
	/// Returns nullptr if the UI system has no GPU rendering (e.g. headless).
	VIRTUAL UIRenderer* METHOD(GetRenderer)() { return nullptr; }
protected:

private:

#pragma endregion

#pragma region MEMBER
public:

protected:

private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE


#endif // !_UIBASE_
