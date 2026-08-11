#pragma once
#ifndef _RMLUISYSTEM_
#define _RMLUISYSTEM_

#include "Core/ConstDefine.h"
#include "UI/UISystem.h"
#include "UI/UIHandleTypes.h"
#include "RmlUIHotReloadService.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/ElementDocument.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Viewport;
class CommandList;
MYRENDERER_END_NAMESPACE
MYRENDERER_BEGIN_NAMESPACE(UI)
class UIRenderer;
class UIInputBridge;
MYRENDERER_BEGIN_NAMESPACE(RmlUI)
class RmlUISystemInterface;
class RmlUIFileInterface;
class RmlUIInputBridge;
class RmlUIRenderInterface;
class RmlUIRenderer;
class RmlDataModelBinder;

// Note: do NOT open a "namespace Rml" block inside MXRender::UI::RmlUI —
// it shadows the global ::Rml namespace.  Use ::Rml:: explicitly.
struct ModelEntry {
	String name;
	::Rml::DataModelConstructor ctor;
	::Rml::DataModelHandle handle;
};
struct DocEntry {
	::Rml::ElementDocument* doc;
	String path;   // LoadPanel path (relative, e.g. "RmlUI/DemoPanel.rml") — hot-reload matching
};

/**
 * RmlUI backend — owns the full RmlUi lifecycle.
 *
 * Non-singleton.  Created by Sample/Editor and passed to UIManager::Create().
 * All RmlUI types are encapsulated; upper layers never include <RmlUi/...>.
 */
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RmlUISystem, public UISystem)

#pragma region METHOD
public:
	RmlUISystem();
	VIRTUAL ~RmlUISystem() OVERRIDE;  // auto-Shutdown

	// ---- UISystem interface ----
	VIRTUAL void METHOD(Init)(RHI::Viewport* viewport) OVERRIDE;
	VIRTUAL void METHOD(Shutdown)() OVERRIDE;
	VIRTUAL void METHOD(Update)(Float32 dt) OVERRIDE;
	VIRTUAL void METHOD(Render)(RHI::CommandList* cmd) OVERRIDE;
	VIRTUAL UIRenderer*    METHOD(GetRenderer)() CONST OVERRIDE;
	VIRTUAL UIInputBridge* METHOD(GetInputBridge)() CONST OVERRIDE;

	VIRTUAL UIModelHandle METHOD(CreateDataModel)(CONST String& name) OVERRIDE;
	VIRTUAL void METHOD(DirtyVariable)(UIModelHandle model, CONST String& name) OVERRIDE;
	VIRTUAL void METHOD(RemoveDataModel)(UIModelHandle model) OVERRIDE;
	VIRTUAL UIDataModelBinder* METHOD(GetModelBinder)(UIModelHandle model) OVERRIDE;
	VIRTUAL void METHOD(BindEventCallback)(UIModelHandle model, CONST String& name,
		std::function<void()> callback) OVERRIDE;

	VIRTUAL UIDocHandle METHOD(LoadPanel)(CONST String& path) OVERRIDE;
	VIRTUAL void METHOD(ShowPanel)(UIDocHandle doc) OVERRIDE;
	VIRTUAL void METHOD(HidePanel)(UIDocHandle doc) OVERRIDE;
	VIRTUAL void METHOD(ClosePanel)(UIDocHandle doc) OVERRIDE;

	VIRTUAL bool METHOD(LoadFont)(CONST String& path) OVERRIDE;
	VIRTUAL bool METHOD(IsMouseInteracting)() CONST OVERRIDE;

	// ---- Dev tooling (modern style) ----
	void EnableHotReload(bool enabled) override;
	void PollHotReload(Float32 dt) override;
	void ToggleDebugger() override;
	void ReloadAllDocuments() override;

protected:
private:
	void METHOD(SetupInterfaces)();
	void METHOD(TeardownInterfaces)();

	/// Apply a hot-reload change set. RENDER thread only (enqueued by PollHotReload).
	void ApplyHotReload(const UIHotReloadChangeSet& changes);

	::Rml::Context* METHOD(GetContext)() CONST { return m_context; }

	/// Returns Rml::DataModelConstructor* for a model handle (internal).
	::Rml::DataModelConstructor* METHOD(GetModelConstructor)(UIModelHandle model);
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
	// Owned interfaces
	RmlUISystemInterface* m_system_interface = nullptr;
	RmlUIFileInterface*   m_file_interface = nullptr;
	RmlUIInputBridge*     m_input_bridge = nullptr;

	// RmlUI context
	::Rml::Context* m_context = nullptr;

	// Renderer (stored as concrete type; GetRenderer() returns UIRenderer*)
	RmlUIRenderer* m_renderer = nullptr;
	RmlUIRenderInterface* m_render_interface = nullptr;

	// Viewport reference
	RHI::Viewport* m_viewport = nullptr;

	// Per-frame accumulated time (replaces glfwGetTime)
	Float64 m_elapsed_time = 0.0;

	bool m_initialized = false;

	// Registries
	Map<GenericHandle, ModelEntry>* m_model_registry = nullptr;
	Map<GenericHandle, DocEntry>*    m_doc_registry = nullptr;

	// Hot-reload file watcher (logic-thread owned; render thread never touches it)
	RmlUIHotReloadService* m_hot_reload_service = nullptr;

	// Next handles
	UInt32 m_next_model_handle = 1;
	UInt32 m_next_doc_handle = 1;

	// Cached binder (one per BindDataModel call, not owned separately)
	UniquePtr<RmlDataModelBinder> m_cached_binder;
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE // RmlUI
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_RMLUISYSTEM_
