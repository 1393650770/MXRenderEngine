#include "UIWidget.h"
#include "UIWidgetManager.h"

#include "UI/UIManager.h"            // global singleton for UI operations
#include "Core/ConstDefine.h"

#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(Widget)

UIWidget::~UIWidget()
{
	if (m_initialized)
		Destroy();
}

void UIWidget::Initialize(UIWidgetManager* manager)
{
	if (m_initialized) return;
	m_manager = manager;

	// Step 1: Create the RmlUI DataModel via UIManager
	CreateDataModel();

	// Step 2: Bind C++ members to the model (generated/manual)
	auto* binder = UIManager::Get().GetModelBinder(m_model_handle);
	RegisterBindings(binder);

	// Step 3: Build the diff tracking table
	PopulateBindings();

	m_initialized = true;
}

void UIWidget::Destroy()
{
	if (!m_initialized) return;

	if (m_model_handle.IsValid())
	{
		UIManager::Get().RemoveDataModel(m_model_handle);
		m_model_handle = {};
	}

	m_bindings.clear();
	m_initialized = false;
}

void UIWidget::SynchronizeProperties()
{
	// Step 1: Apply cross-thread queued writes
	FlushCommandQueue();

	// Step 2: RTTR diff → DirtyVariable for changed OneWay bindings
	for (auto& [display_name, entry] : m_bindings)
	{
		if (entry.mode != EBindingMode::OneWay) continue;
		if (!entry.property.is_valid()) continue;

		rttr::variant current = entry.property.get_value(
			rttr::instance(this));  // HACK: 'this' is the concrete type?
		// Note: rttr::instance(this) only works if 'this' is the concrete
		// registered type.  Since CreateWidget<T> passes rttr::type::get<T>(),
		// and T is the final concrete class, property lookup works.

		if (current != entry.previous_value)
		{
			UIManager::Get().DirtyVariable(m_model_handle, display_name);
			entry.previous_value = current;
			OnPropertyChanged(display_name);
		}
	}
}

void UIWidget::CreateDataModel()
{
	if (!m_concrete_type.is_valid())
	{
		std::cerr << "[UIWidget] m_concrete_type not set — did you forget "
			"CreateWidget<T>()?" << std::endl;
		return;
	}

	// Default model name from RTTR type name
	if (m_model_name.empty())
		m_model_name = String(m_concrete_type.get_name().data());

	m_model_handle = UIManager::Get().CreateDataModel(m_model_name);
}

void UIWidget::RegisterBindings(UI::UIDataModelBinder* binder)
{
	// Default is no-op — derived class overrides or
	// uses UIWidgetBindingTraits<T>::BindDataModel via the factory pointer.
	(void)binder;
}

void UIWidget::PopulateBindings()
{
	if (!m_concrete_type.is_valid()) return;

	// Default: read from UIWidgetBindingTraits<T>.
	// In Phase 1, traits::GetBindingEntries returns empty.
	// In Phase 2, MetaParser generates the entries from annotations.
	// For now, derived classes can override PopulateBindings to fill
	// the table manually.
}

void UIWidget::OnPropertyChanged(CONST String& display_name)
{
	(void)display_name;
}

void UIWidget::MarkDirty(CONST String& display_name)
{
	auto it = m_bindings.find(display_name);
	if (it == m_bindings.end()) return;

	UIManager::Get().DirtyVariable(m_model_handle, display_name);

	// Refresh previous_value from current to prevent re-triggering
	if (it->second.property.is_valid())
		it->second.previous_value = it->second.property.get_value(
			rttr::instance(this));
}

// -----------------------------------------------------------------------
// Thread-safe command queue
// -----------------------------------------------------------------------

void UIWidget::EnqueuePropertyChange(CONST String& name, rttr::variant value)
{
	std::lock_guard<std::mutex> lock(m_cmd_mutex);
	m_cmd_queue.push_back({ name, std::move(value) });
	m_has_pending = true;
}

void UIWidget::FlushCommandQueue()
{
	if (!m_has_pending) return;

	Vector<PropertyCommand> commands;
	{
		std::lock_guard<std::mutex> lock(m_cmd_mutex);
		commands.swap(m_cmd_queue);
		m_has_pending = false;
	}

	for (auto& cmd : commands)
	{
		auto it = m_bindings.find(cmd.name);
		if (it != m_bindings.end() && it->second.property.is_valid())
		{
			it->second.property.set_value(
				rttr::instance(this), cmd.value);
		}
	}
}

MYRENDERER_END_NAMESPACE // Widget
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
