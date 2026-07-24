#include "UIManager.h"

// Only this .cpp file knows about the RmlUI backend.
// All other Runtime and Application code goes through UIManager.
#include "UI/RmlUI/RmlUIManager.h"
#include "UI/RmlUI/RmlUIInputBridge.h"  // full type for inheritance upcast

#include <RmlUi/Core/DataModelHandle.h>  // for Rml::DataEventFunc cast
#include <RmlUi/Debugger.h>              // for EnableDebugger
#include "RHI/RenderViewport.h"
#include "RHI/RenderCommandList.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

// ---------------------------------------------------------------------------
// Pimpl — backend pointer hidden from header
// ---------------------------------------------------------------------------

class UIManager::Impl
{
public:
	MXRender::UI::RmlUI::RmlUIManager* backend = nullptr;
};

UIManager* UIManager::s_instance = nullptr;

// ---------------------------------------------------------------------------
// Singleton lifecycle
// ---------------------------------------------------------------------------

UIManager& UIManager::Get()
{
	return *s_instance;
}

void UIManager::Create(RHI::Viewport* viewport)
{
	if (s_instance) return;

	s_instance = new UIManager();
	s_instance->m_impl = new Impl();

	s_instance->m_impl->backend = MXRender::UI::RmlUI::RmlUIManager::Get();
	s_instance->m_impl->backend->Init(viewport);
}

void UIManager::Destroy()
{
	if (!s_instance) return;

	MXRender::UI::RmlUI::RmlUIManager::Shutdown();

	delete s_instance->m_impl;
	s_instance->m_impl = nullptr;

	delete s_instance;
	s_instance = nullptr;
}

// ---------------------------------------------------------------------------
// Per-frame lifecycle
// ---------------------------------------------------------------------------

void UIManager::ProcessInput()    { m_impl->backend->ProcessInput(); }
void UIManager::Update(Float32 dt) { m_impl->backend->Update(dt); }

void UIManager::Render(RHI::CommandList* cmd)
{
	m_impl->backend->Render(cmd);
}

// ---------------------------------------------------------------------------
// Sub-system access
// ---------------------------------------------------------------------------

UIRenderer* UIManager::GetRenderer() CONST
{
	return m_impl->backend->GetRenderer();
}

UIInputBridge* UIManager::GetInputBridge() CONST
{
	// RmlUIInputBridge publicly inherits UIInputBridge → implicit upcast
	return m_impl->backend->GetInputBridge();
}

// ---------------------------------------------------------------------------
// Data model — UIModelHandle == RmlModelHandle (same type via alias)
// ---------------------------------------------------------------------------

UIModelHandle UIManager::CreateDataModel(CONST String& name, bool allow_missing)
{
	return m_impl->backend->CreateDataModel(name, allow_missing);
}

void UIManager::BindEventCallbackImpl(UIModelHandle model, CONST String& name,
	void* func_impl)
{
	auto& func = *static_cast<Rml::DataEventFunc*>(func_impl);
	m_impl->backend->BindEventCallback(model, name, std::move(func));
}

void UIManager::DirtyVariable(UIModelHandle model, CONST String& name)
{
	m_impl->backend->DirtyVariable(model, name);
}

void UIManager::RemoveDataModel(UIModelHandle model)
{
	m_impl->backend->RemoveDataModel(model);
}

void* UIManager::GetModelConstructor(UIModelHandle model)
{
	return m_impl->backend->GetModelConstructor(model);
}

// ---------------------------------------------------------------------------
// Document management — UIDocHandle == RmlDocHandle (same type via alias)
// ---------------------------------------------------------------------------

UIDocHandle UIManager::LoadDocument(CONST String& path)
{
	return m_impl->backend->LoadDocument(path);
}

void UIManager::ShowDocument(UIDocHandle doc)   { m_impl->backend->ShowDocument(doc); }
void UIManager::HideDocument(UIDocHandle doc)   { m_impl->backend->HideDocument(doc); }

void UIManager::CloseDocument(UIDocHandle doc)
{
	m_impl->backend->CloseDocument(doc);
}

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

bool UIManager::LoadFontFace(CONST String& file_path)
{
	return m_impl->backend->LoadFontFace(file_path);
}

bool UIManager::IsMouseInteracting() CONST
{
	return m_impl->backend->IsMouseInteracting();
}

void UIManager::EnableDebugger(bool enable)
{
	if (enable)
	{
		auto* ctx = m_impl->backend->GetContext();
		if (ctx && !Rml::Debugger::IsVisible())
			Rml::Debugger::Initialise(ctx);
	}
	else
	{
		Rml::Debugger::Shutdown();
	}
}

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
