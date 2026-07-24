#include "UIManager.h"
#include "UI/UISystem.h"
#include "UI/UIDataModelBinder.h"
#include "UI/UIRenderer.h"
#include "UI/UIInputBridge.h"
#include "RHI/RenderCommandList.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

// =========================================================================
// Singleton lifecycle
// =========================================================================

UIManager* UIManager::s_instance = nullptr;

UIManager& UIManager::Get()
{
	return *s_instance;
}

void UIManager::Create(UISystem* backend)
{
	if (s_instance || !backend) return;

	s_instance = new UIManager();
	s_instance->m_backend = backend;
}

void UIManager::Destroy()
{
	if (!s_instance) return;

	if (s_instance->m_backend)
	{
		s_instance->m_backend->Shutdown();
		delete s_instance->m_backend;
		s_instance->m_backend = nullptr;
	}

	delete s_instance;
	s_instance = nullptr;
}

// =========================================================================
// Per-frame
// =========================================================================

void UIManager::Update(Float32 dt)
{
	if (m_backend) m_backend->Update(dt);
}

void UIManager::Render(RHI::CommandList* cmd)
{
	if (m_backend) m_backend->Render(cmd);
}

// =========================================================================
// Sub-system access
// =========================================================================

UIRenderer* UIManager::GetRenderer() CONST
{
	return m_backend ? m_backend->GetRenderer() : nullptr;
}

UIInputBridge* UIManager::GetInputBridge() CONST
{
	return m_backend ? m_backend->GetInputBridge() : nullptr;
}

// =========================================================================
// Data model
// =========================================================================

UIModelHandle UIManager::CreateDataModel(CONST String& name)
{
	return m_backend ? m_backend->CreateDataModel(name) : UIModelHandle{};
}

void UIManager::DirtyVariable(UIModelHandle model, CONST String& name)
{
	if (m_backend) m_backend->DirtyVariable(model, name);
}

void UIManager::RemoveDataModel(UIModelHandle model)
{
	if (m_backend) m_backend->RemoveDataModel(model);
}

UIDataModelBinder* UIManager::GetModelBinder(UIModelHandle model)
{
	return m_backend ? m_backend->GetModelBinder(model) : nullptr;
}

void UIManager::BindEventCallback(UIModelHandle model, CONST String& name,
	std::function<void()> callback)
{
	if (m_backend) m_backend->BindEventCallback(model, name, std::move(callback));
}

// =========================================================================
// Document / Panel
// =========================================================================

UIDocHandle UIManager::LoadPanel(CONST String& path)
{
	return m_backend ? m_backend->LoadPanel(path) : UIDocHandle{};
}

void UIManager::ShowPanel(UIDocHandle doc)
{
	if (m_backend) m_backend->ShowPanel(doc);
}

void UIManager::HidePanel(UIDocHandle doc)
{
	if (m_backend) m_backend->HidePanel(doc);
}

void UIManager::ClosePanel(UIDocHandle doc)
{
	if (m_backend) m_backend->ClosePanel(doc);
}

// =========================================================================
// Resources / Query
// =========================================================================

bool UIManager::LoadFont(CONST String& file_path)
{
	return m_backend ? m_backend->LoadFont(file_path) : false;
}

bool UIManager::IsMouseInteracting() CONST
{
	return m_backend ? m_backend->IsMouseInteracting() : false;
}

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
