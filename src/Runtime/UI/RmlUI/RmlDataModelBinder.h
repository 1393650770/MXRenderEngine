#pragma once
#ifndef _RMLDATAMODELBINDER_
#define _RMLDATAMODELBINDER_

#include "Core/ConstDefine.h"
#include "UI/UIDataModelBinder.h"

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/DataTypes.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(RmlUI)

/**
 * Concrete UIDataModelBinder — wraps Rml::DataModelConstructor.
 *
 * Translates abstract Bind/BindTwoWay/BindEvent calls into RmlUi API calls.
 * Event callbacks are wrapped from std::function<void()> into Rml::DataEventFunc,
 * stripping all Rml-specific parameters before calling application code.
 */
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RmlDataModelBinder, public UIDataModelBinder)

#pragma region METHOD
public:
	explicit RmlDataModelBinder(::Rml::DataModelConstructor* ctor) : m_ctor(ctor) {}
	VIRTUAL ~RmlDataModelBinder() MYDEFAULT;

	// OneWay
	VIRTUAL void METHOD(Bind)(CONST String& name, Int* ptr)       OVERRIDE FINAL { if (m_ctor) m_ctor->Bind(name, ptr); }
	VIRTUAL void METHOD(Bind)(CONST String& name, Float32* ptr)   OVERRIDE FINAL { if (m_ctor) m_ctor->Bind(name, ptr); }
	VIRTUAL void METHOD(Bind)(CONST String& name, String* ptr)    OVERRIDE FINAL { if (m_ctor) m_ctor->Bind(name, ptr); }
	VIRTUAL void METHOD(Bind)(CONST String& name, Bool* ptr)      OVERRIDE FINAL { if (m_ctor) m_ctor->Bind(name, ptr); }

	// TwoWay — wraps typed getter/setter into Rml::Variant
	VIRTUAL void METHOD(BindTwoWay)(CONST String& name,
		std::function<Int()> getter, std::function<void(Int)> setter) OVERRIDE FINAL
	{
		if (m_ctor) m_ctor->BindFunc(name,
			[g = std::move(getter)](::Rml::Variant& out) { out = g(); },
			[s = std::move(setter)](const ::Rml::Variant& in) { s(in.Get<Int>()); });
	}
	VIRTUAL void METHOD(BindTwoWay)(CONST String& name,
		std::function<Float32()> getter, std::function<void(Float32)> setter) OVERRIDE FINAL
	{
		if (m_ctor) m_ctor->BindFunc(name,
			[g = std::move(getter)](::Rml::Variant& out) { out = g(); },
			[s = std::move(setter)](const ::Rml::Variant& in) { s(in.Get<Float32>()); });
	}
	VIRTUAL void METHOD(BindTwoWay)(CONST String& name,
		std::function<String()> getter, std::function<void(CONST String&)> setter) OVERRIDE FINAL
	{
		if (m_ctor) m_ctor->BindFunc(name,
			[g = std::move(getter)](::Rml::Variant& out) { out = g().c_str(); },
			[s = std::move(setter)](const ::Rml::Variant& in) { s(String(in.Get<const char*>())); });
	}
	VIRTUAL void METHOD(BindTwoWay)(CONST String& name,
		std::function<Bool()> getter, std::function<void(Bool)> setter) OVERRIDE FINAL
	{
		if (m_ctor) m_ctor->BindFunc(name,
			[g = std::move(getter)](::Rml::Variant& out) { out = g(); },
			[s = std::move(setter)](const ::Rml::Variant& in) { s(in.Get<Bool>()); });
	}

	// Event — wraps plain callback into Rml::DataEventFunc, stripping all Rml params
	VIRTUAL void METHOD(BindEvent)(CONST String& name, std::function<void()> callback) OVERRIDE FINAL
	{
		if (m_ctor) m_ctor->BindEventCallback(name,
			[cb = std::move(callback)](::Rml::DataModelHandle, ::Rml::Event&, const ::Rml::VariantList&) { cb(); });
	}

protected:
private:
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
	::Rml::DataModelConstructor* m_ctor = nullptr;
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE // RmlUI
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_RMLDATAMODELBINDER_
