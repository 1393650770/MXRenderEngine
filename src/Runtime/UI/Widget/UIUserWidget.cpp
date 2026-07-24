#include "UIUserWidget.h"
#include "UIWidgetManager.h"

#include "UI/UIManager.h"
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(Widget)

UIUserWidget::~UIUserWidget()
{
	if (m_initialized)
		Destroy();
}

void UIUserWidget::Initialize(UIWidgetManager* manager)
{
	// Step 1: Base creates DataModel + registers bindings
	UIWidget::Initialize(manager);

	// Step 2: Load the RML document
	if (!m_rml_path.empty())
	{
		m_doc_handle = UIManager::Get().LoadPanel(m_rml_path);
		if (m_doc_handle.IsValid())
		{
			if (m_auto_show)
				UIManager::Get().ShowPanel(m_doc_handle);

			std::cout << "[UIUserWidget] Loaded panel: " << m_rml_path << std::endl;
		}
		else
		{
			std::cerr << "[UIUserWidget] Failed to load: " << m_rml_path << std::endl;
		}
	}
}

void UIUserWidget::Destroy()
{
	if (m_doc_handle.IsValid())
	{
		UIManager::Get().ClosePanel(m_doc_handle);
		m_doc_handle = {};
	}

	// Destroy children in reverse order
	m_child_widgets.clear();

	UIWidget::Destroy();
}

void UIUserWidget::Show()
{
	if (m_doc_handle.IsValid())
		UIManager::Get().ShowPanel(m_doc_handle);
}

void UIUserWidget::Hide()
{
	if (m_doc_handle.IsValid())
		UIManager::Get().HidePanel(m_doc_handle);
}

void UIUserWidget::Close()
{
	if (m_doc_handle.IsValid())
	{
		UIManager::Get().ClosePanel(m_doc_handle);
		m_doc_handle = {};
	}
}

MYRENDERER_END_NAMESPACE // Widget
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
