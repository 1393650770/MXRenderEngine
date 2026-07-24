#pragma once
#ifndef _UIWIDGETBINDING_
#define _UIWIDGETBINDING_

#include "Core/ConstDefine.h"

// Undo windows.h min/max macros that break RTTR's std::min/std::max.
// Must appear AFTER ConstDefine.h (and whatever it transitively includes).
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <string>
#include <vector>
#include <map>
#include <unordered_map>

#include <rttr/type.h>
#include <rttr/variant.h>
#include <rttr/property.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
class UIDataModelBinder;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(Widget)

/// How a C++ property maps to a RmlUI data model variable.
enum class EBindingMode : uint8_t
{
	OneWay,   // C++ → RmlUI, auto-diff each frame
	TwoWay,   // C++ ↔ RmlUI via BindFunc getter/setter
	OneShot,  // C++ → RmlUI, init only (no per-frame tracking)
};

/// One binding entry in a widget's binding table.
/// Filled by PopulateBindings() which reads metadata (Phase 1: manual;
/// Phase 2: from MetaParser-generated UIWidgetBindingTraits<T>).
struct UIBindingEntry
{
	String          display_name;    // RmlUI variable name (e.g. "hp")
	rttr::property  property;        // RTTR property handle
	EBindingMode    mode = EBindingMode::OneWay;
	rttr::variant   previous_value;  // For OneWay diff detection
};

using UIBindingTable = Map<String, UIBindingEntry>;  // keyed by display_name

/// Primary template — unspecialized means "no bindings".
/// MetaParser (Phase 2) generates specializations for each reflected class.
template<typename T>
struct UIWidgetBindingTraits
{
	/// Called by UIWidget::RegisterBindings() to bind C++ members to the
	/// UI data model via the abstract DataModelBinder.
	/// @param binder  Backend-specific UIDataModelBinder instance.
	static void BindDataModel(UI::UIDataModelBinder* binder, T* data) { (void)binder; (void)data; }

	/// Returns the binding table for RTTR-based auto-diff.
	static Vector<UIBindingEntry> GetBindingEntries(const rttr::type& type)
	{
		(void)type;
		return {};
	}

	/// Returns per-field metadata (display names, clamp ranges, categories).
	/// Used by Editor property panels in Phase 3.
	static Map<String, Map<String, String>> GetFieldMetadata() { return {}; }
};

MYRENDERER_END_NAMESPACE // Widget
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UIWIDGETBINDING_
