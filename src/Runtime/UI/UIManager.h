#pragma once
#ifndef _UIMANAGER_
#define _UIMANAGER_

#include "Core/ConstDefine.h"
#include "UI/UIHandleTypes.h"       // UIModelHandle, UIDocHandle

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Viewport;
class CommandList;
MYRENDERER_END_NAMESPACE
MYRENDERER_BEGIN_NAMESPACE(UI)
class UIRenderer;
class UIInputBridge;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

/**
 * Global UI Manager — backend-agnostic singleton facade.
 *
 * Application layer uses ONLY this class.  The concrete backend (RmlUI)
 * is hidden behind a Pimpl — no backend types leak through this header.
 *
 * For event callback binding, use generated Bind*() functions or the
 * MXWidget framework.  Manual callbacks can include <RmlUi/Core/...> and
 * call GetModelConstructor() which returns an opaque void* pointer.
 *
 * Usage:
 *   UIManager::Create(viewport);
 *   auto h = UIManager::Get().CreateDataModel("hud");
 *   auto doc = UIManager::Get().LoadDocument("p.rml");
 *   UIManager::Get().ShowDocument(doc);
 *   // per frame: ProcessInput() → Update(dt) → Render(cmd)
 *   UIManager::Destroy();
 */
MYRENDERER_BEGIN_CLASS(UIManager)
#pragma region METHOD
public:
	static UIManager& METHOD(Get)();
	static void METHOD(Create)(RHI::Viewport* viewport);
	static void METHOD(Destroy)();

	// Per-frame
	void METHOD(ProcessInput)();
	void METHOD(Update)(Float32 dt);
	void METHOD(Render)(RHI::CommandList* cmd);

	// Sub-systems
	UIRenderer*    METHOD(GetRenderer)() CONST;
	UIInputBridge* METHOD(GetInputBridge)() CONST;

	// Data model
	UIModelHandle METHOD(CreateDataModel)(CONST String& name, bool allow_missing = false);
	void METHOD(DirtyVariable)(UIModelHandle model, CONST String& name);
	void METHOD(RemoveDataModel)(UIModelHandle model);

	/// @internal Bind an event callback.  The func type must match the
	/// backend's expected signature.  Application code should use generated
	/// Bind*() functions or MXWidget instead of calling this directly.
	/// @param func_impl  Opaque pointer to a std::function with the backend's
	///                    DataEventFunc signature (Rml::DataEventFunc).
	void METHOD(BindEventCallbackImpl)(UIModelHandle model, CONST String& name,
		void* func_impl);

	// Document
	UIDocHandle METHOD(LoadDocument)(CONST String& path);
	void METHOD(ShowDocument)(UIDocHandle doc);
	void METHOD(HideDocument)(UIDocHandle doc);
	void METHOD(CloseDocument)(UIDocHandle doc);

	// Resources
	bool METHOD(LoadFontFace)(CONST String& file_path);

	// Query
	bool METHOD(IsMouseInteracting)() CONST;

	// Debug — enable the backend's visual debugger (RmlUi: F8 to toggle)
	void METHOD(EnableDebugger)(bool enable);

	/// @internal Returns an opaque pointer to the backend model constructor.
	/// For use by generated Bind*() functions.  Cast to Rml::DataModelConstructor*
	/// in code that knows the backend type.
	void* METHOD(GetModelConstructor)(UIModelHandle model);

private:
	UIManager() MYDEFAULT;
	~UIManager() MYDEFAULT;

	class Impl;
	Impl* m_impl = nullptr;
	static UIManager* s_instance;

	UIManager(CONST UIManager&) MYDELETE;
	UIManager& operator=(CONST UIManager&) MYDELETE;
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UIMANAGER_
