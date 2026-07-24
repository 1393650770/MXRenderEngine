#pragma once
#ifndef _UIMANAGER_
#define _UIMANAGER_

#include "Core/ConstDefine.h"
#include "UI/UIHandleTypes.h"
#include "UI/UISystem.h"       // for UISystem* — abstract base, zero backend deps
#include <functional>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class CommandList;
MYRENDERER_END_NAMESPACE
MYRENDERER_BEGIN_NAMESPACE(UI)
class UIRenderer;
class UIInputBridge;
class UIDataModelBinder;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

/**
 * Global UI Manager — backend-agnostic singleton facade.
 *
 * Application layer uses ONLY this class.  The concrete backend (RmlUISystem,
 * future ImGuiSystem, etc.) is passed via Create() and stored as a UISystem*.
 *
 * Usage:
 *   auto* ui = new RmlUI::RmlUISystem();
 *   UIManager::Create(ui);
 *   auto h = UIManager::Get().CreateDataModel("hud");
 *   UIManager::Get().BindDataModel<Traits>(h, &data);
 *   UIManager::Get().BindEventCallback(h, "ev", []{ ... });
 *   auto doc = UIManager::Get().LoadPanel("p.rml");
 *   UIManager::Get().ShowPanel(doc);
 *   // per frame: Update(dt) → Render(cmd)
 *   UIManager::Destroy();
 */
MYRENDERER_BEGIN_CLASS(UIManager)
#pragma region METHOD
public:
	static UIManager& METHOD(Get)();
	static void METHOD(Create)(UISystem* backend);
	static void METHOD(Destroy)();

	// Per-frame
	void METHOD(Update)(Float32 dt);
	void METHOD(Render)(RHI::CommandList* cmd);

	// Sub-systems
	UIRenderer*    METHOD(GetRenderer)() CONST;
	UIInputBridge* METHOD(GetInputBridge)() CONST;

	// Data model
	UIModelHandle METHOD(CreateDataModel)(CONST String& name);
	void METHOD(DirtyVariable)(UIModelHandle model, CONST String& name);
	void METHOD(RemoveDataModel)(UIModelHandle model);

	/// Returns a backend-specific DataModelBinder (for Widget framework RTTR path).
	UIDataModelBinder* METHOD(GetModelBinder)(UIModelHandle model);

	/// Bind C++ members to a data model via generated UIWidgetBindingTraits<T>.
	/// Template path (Sample usage) — fully type-safe, zero void*.
	template<typename Traits, typename T>
	void METHOD(BindDataModel)(UIModelHandle model, T* data)
	{
		if (m_backend)
		{
			auto* binder = m_backend->GetModelBinder(model);
			if (binder) Traits::BindDataModel(binder, data);
		}
	}

	/// Bind a plain std::function<void()> event callback.
	void METHOD(BindEventCallback)(UIModelHandle model, CONST String& name,
		std::function<void()> callback);

	// Document / Panel
	UIDocHandle METHOD(LoadPanel)(CONST String& path);
	void METHOD(ShowPanel)(UIDocHandle doc);
	void METHOD(HidePanel)(UIDocHandle doc);
	void METHOD(ClosePanel)(UIDocHandle doc);

	// Resources
	bool METHOD(LoadFont)(CONST String& file_path);

	// Query
	bool METHOD(IsMouseInteracting)() CONST;

private:
	UIManager() MYDEFAULT;
	~UIManager() MYDEFAULT;

	UISystem* m_backend = nullptr;
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
