#pragma once
#ifndef _UISYSTEM_
#define _UISYSTEM_

#include "Core/ConstDefine.h"
#include "UI/UIHandleTypes.h"
#include <functional>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Viewport;
class CommandList;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
class UIRenderer;
class UIInputBridge;
class UIDataModelBinder;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

/**
 * Base class for all UI backends (RmlUI, ImGui, future).
 *
 * Patterned after RHI::RenderRHI — virtual methods with default empty
 * implementations so backends only override what they need.
 *
 * Not a singleton.  Created by the application and passed to UIManager::Create().
 */
MYRENDERER_BEGIN_CLASS(UISystem)
#pragma region METHOD
public:
	UISystem() MYDEFAULT;
	VIRTUAL ~UISystem() MYDEFAULT;

	// ---- Lifecycle ----
	VIRTUAL void METHOD(Init)(RHI::Viewport* viewport) {}
	VIRTUAL void METHOD(Shutdown)() {}

	// ---- Per-frame ----
	VIRTUAL void METHOD(Update)(Float32 dt) {}
	VIRTUAL void METHOD(Render)(RHI::CommandList* cmd) {}

	// ---- Sub-system access ----
	VIRTUAL UIRenderer*    METHOD(GetRenderer)() CONST { return nullptr; }
	VIRTUAL UIInputBridge* METHOD(GetInputBridge)() CONST { return nullptr; }

	// ---- Data model ----
	VIRTUAL UIModelHandle METHOD(CreateDataModel)(CONST String& name) { (void)name; return {}; }
	VIRTUAL void METHOD(DirtyVariable)(UIModelHandle model, CONST String& name) {}
	VIRTUAL void METHOD(RemoveDataModel)(UIModelHandle model) {}

	/// Returns a backend-specific DataModelBinder for BindDataModel templates.
	VIRTUAL UIDataModelBinder* METHOD(GetModelBinder)(UIModelHandle model) { (void)model; return nullptr; }

	/// Bind a plain std::function<void()> event callback.
	VIRTUAL void METHOD(BindEventCallback)(UIModelHandle model, CONST String& name,
		std::function<void()> callback) {}

	// ---- Document / Panel ----
	VIRTUAL UIDocHandle METHOD(LoadPanel)(CONST String& path) { (void)path; return {}; }
	VIRTUAL void METHOD(ShowPanel)(UIDocHandle doc) {}
	VIRTUAL void METHOD(HidePanel)(UIDocHandle doc) {}
	VIRTUAL void METHOD(ClosePanel)(UIDocHandle doc) {}

	// ---- Resources ----
	VIRTUAL bool METHOD(LoadFont)(CONST String& path) { (void)path; return false; }
	VIRTUAL bool METHOD(IsMouseInteracting)() CONST { return false; }

protected:
private:
	UISystem(CONST UISystem&) MYDELETE;
	UISystem& operator=(CONST UISystem&) MYDELETE;
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UISYSTEM_
