#pragma once
#ifndef _UIWIDGET_
#define _UIWIDGET_

#include "Core/ConstDefine.h"
#include "UI/UIHandleTypes.h"
#include "UI/Widget/UIWidgetBinding.h"

#include <mutex>
#include <vector>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
class UIDataModelBinder;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(Widget)

class UIWidgetManager;

/**
 * Abstract base for a reflection-driven UI data model wrapper.
 *
 * Each UIWidget owns one RmlUI DataModel.  Properties annotated with
 * UI_BIND_FIELD_AS / UI_BIND_TWO_WAY are automatically bound and
 * synchronized each frame via RTTR diff detection.
 *
 * Thread model: Direct property writes (widget->m_hp = 80) are main-thread
 * only (zero overhead).  Cross-thread updates use EnqueuePropertyChange()
 * which is flushed at the start of SynchronizeProperties().
 */
MYRENDERER_BEGIN_CLASS(UIWidget)
#pragma region METHOD
public:
	UIWidget() MYDEFAULT;
	VIRTUAL ~UIWidget();

	/// Initialize: create DataModel, register bindings, populate diff table.
	/// Called by UIWidgetManager::CreateWidget().
	VIRTUAL void METHOD(Initialize)(UIWidgetManager* manager);

	/// Remove DataModel.  Called by UIWidgetManager destructor.
	VIRTUAL void METHOD(Destroy)();

	/// Per-frame: flush cross-thread commands, diff OneWay properties,
	/// call DirtyVariable() for changed values.  Must be called on main thread.
	VIRTUAL void METHOD(SynchronizeProperties)();

	// ── Thread-safe API (opt-in for cross-thread updates) ────────────

	/// Enqueue a property change from any thread.  Thread-safe.
	/// Changes are applied when FlushCommandQueue() runs on the main thread.
	void METHOD(EnqueuePropertyChange)(CONST String& name, rttr::variant value);

	/// Apply all queued property changes (main thread only).
	void METHOD(FlushCommandQueue)();

	// ── Manual control ───────────────────────────────────────────────

	/// Force-mark a property as dirty (bypass RTTR diff).
	void METHOD(MarkDirty)(CONST String& display_name);

	// ── Accessors ─────────────────────────────────────────────────────

	UIModelHandle   METHOD(GetModelHandle)() CONST { return m_model_handle; }
	CONST String&   METHOD(GetModelName)()  CONST { return m_model_name; }
	UIWidgetManager* METHOD(GetManager)()   CONST { return m_manager; }
	bool            METHOD(IsInitialized)() CONST { return m_initialized; }
	CONST rttr::type& METHOD(GetConcreteType)() CONST { return m_concrete_type; }

protected:
	/// Called by the factory template BEFORE Initialize() to set the
	/// concrete RTTR type.  Derived class constructors should call this.
	void METHOD(SetConcreteType)(CONST rttr::type& t) { m_concrete_type = t; }
	/// Override to add custom bindings beyond the generated ones.
	/// Default reads UIWidgetBindingTraits<T>::BindDataModel().
	/// @param binder  Backend-specific UIDataModelBinder instance.
	VIRTUAL void METHOD(RegisterBindings)(UI::UIDataModelBinder* binder);

	/// Called after a property is changed (OneWay diff or TwoWay set).
	VIRTUAL void METHOD(OnPropertyChanged)(CONST String& display_name);

	/// Internal: build the binding table from UIWidgetBindingTraits<T>.
	void METHOD(PopulateBindings)();

	/// Create the RmlUI DataModel via UIManager.
	void METHOD(CreateDataModel)();

	UIBindingTable m_bindings;       // display_name → binding entry
	String         m_model_name;     // defaults to class name (rttr::type::get_name)
	rttr::type     m_concrete_type;  // set by factory before Initialize()
	UIModelHandle  m_model_handle;
	UIWidgetManager* m_manager = nullptr;
	bool           m_initialized = false;

private:
	UIWidget(CONST UIWidget&) MYDELETE;
	UIWidget& operator=(CONST UIWidget&) MYDELETE;

	struct PropertyCommand {
		String name;
		rttr::variant value;
	};
	std::mutex            m_cmd_mutex;
	Vector<PropertyCommand> m_cmd_queue;  // guarded by m_cmd_mutex
	bool                  m_has_pending = false;
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE // Widget
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UIWIDGET_
