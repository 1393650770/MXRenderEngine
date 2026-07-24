#include "RmlUIManager.h"
#include "RmlUISystemInterface.h"
#include "RmlUIFileInterface.h"
#include "RmlUIInputBridge.h"
#include "RmlUIRenderer.h"
#include "RmlUIRenderInterface.h"
#include "UI/UIRenderer.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Debugger.h>

#include "RHI/RenderViewport.h"
#include "RHI/RenderTexture.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderCommandList.h"
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(RmlUI)

RmlUIManager* RmlUIManager::s_instance = nullptr;

RmlUIManager* RmlUIManager::Get()
{
	if (!s_instance)
		s_instance = new RmlUIManager();
	return s_instance;
}

RmlUIManager::~RmlUIManager()
{
	// NO-OP: Shutdown must be called explicitly as a static method
	// before vkDestroyDevice.  Instance dtors run too late (after RHI
	// has been shut down) and would trip buffer-leak asserts.
}

void RmlUIManager::Init(RHI::Viewport* viewport)
{
	if (m_initialized) return;
	m_viewport = viewport;

	// Allocate internal registries
	m_model_registry = new Map<GenericHandle, ModelEntry>();
	m_doc_registry = new Map<GenericHandle, DocEntry>();

	SetupInterfaces();

	// Create renderer and install BEFORE Rml::CreateContext (RmlUI requires it)
	m_renderer = new RmlUIRenderer();
	m_render_interface = new RmlUIRenderInterface(m_renderer);
	Rml::SetRenderInterface(static_cast<Rml::RenderInterface*>(m_render_interface->GetRmlInterface()));

	Rml::Initialise();

	auto size = viewport->GetViewportSize();
	Rml::Vector2i dims(static_cast<int>(size[0]), static_cast<int>(size[1]));
	m_context = Rml::CreateContext("main", dims);

	if (!m_context)
	{
		std::cerr << "[RmlUIManager] Failed to create RmlUI context!" << std::endl;
		return;
	}

	auto* ctx = m_context;
	std::cout << "[RmlUIManager] Initialized — context \"" << ctx->GetName()
		<< "\" " << dims.x << "x" << dims.y << std::endl;

	m_input_bridge = new RmlUIInputBridge();
	m_input_bridge->SetContext(ctx);

	// Initialize renderer GPU resources (shaders, PSOs)
	auto* cmd = RHIGetImmediateCommandList();
	auto* bb_rtv = viewport->GetCurrentBackBufferRTV();
	auto* bb_dsv = viewport->GetCurrentBackBufferDSV();
	static_cast<RmlUIRenderer*>(m_renderer)->Initialize(cmd, bb_rtv, bb_dsv, size[0], size[1]);
	static_cast<RmlUIRenderer*>(m_renderer)->SetViewport(viewport);

	m_initialized = true;
}

void RmlUIManager::Shutdown()
{
	if (!s_instance || !s_instance->m_initialized) return;
	auto* mgr = s_instance;

	// 1. Rml::Shutdown() — calls back render interface's ReleaseGeometry/ReleaseTexture.
	//    Renderer and interface MUST still be alive.
	Rml::Shutdown();

	// 2. Release render interface
	delete mgr->m_render_interface; mgr->m_render_interface = nullptr;

	// 3. Release renderer (GPU resources — GPU stopped by now, safe to delete)
	delete mgr->m_renderer; mgr->m_renderer = nullptr;

	// 4. Release input bridge
	delete mgr->m_input_bridge; mgr->m_input_bridge = nullptr;

	// 5. Teardown system/file interfaces
	mgr->TeardownInterfaces();

	// 6. Delete registries
	delete mgr->m_model_registry; mgr->m_model_registry = nullptr;
	delete mgr->m_doc_registry;   mgr->m_doc_registry = nullptr;

	mgr->m_initialized = false;
	mgr->m_context = nullptr;

	// 7. Destroy singleton
	delete s_instance;
	s_instance = nullptr;

	std::cout << "[RmlUIManager] Shutdown complete." << std::endl;
}

void RmlUIManager::ProcessInput()
{
	// Input is processed externally via RmlUIInputBridge
}

void RmlUIManager::Update(Float32 dt)
{
	auto* ctx = m_context;
	if (!ctx) return;

	auto size = m_viewport->GetViewportSize();
	ctx->SetDimensions(Rml::Vector2i(static_cast<int>(size[0]), static_cast<int>(size[1])));
	ctx->Update();
}

void RmlUIManager::Render(RHI::CommandList* cmd)
{
	auto* ctx = m_context;
	if (!ctx || !m_renderer) return;

	m_renderer->BeginFrame(cmd);
	ctx->Render();
	m_renderer->EndFrame(cmd);
}

// -----------------------------------------------------------------------
// Data model binding
// -----------------------------------------------------------------------
RmlModelHandle RmlUIManager::CreateDataModel(CONST String& name, bool allow_missing)
{
	auto* ctx = m_context;
	if (!ctx) return {};

	auto ctor = ctx->CreateDataModel(name, nullptr, allow_missing);
	if (!ctor) return {};

	RmlModelHandle h; h.value = m_next_model_handle++;
	auto* reg = m_model_registry;
	ModelEntry entry;
	entry.name = name;
	entry.ctor = ctor;
	entry.handle = ctor.GetModelHandle();
	(*reg)[h.value] = entry;
	return h;
}

void RmlUIManager::BindEventCallback(RmlModelHandle model, CONST String& name,
	Rml::DataEventFunc func)
{
	auto* ctor = GetModelConstructor(model);
	if (ctor) ctor->BindEventCallback(name, func);
}

void RmlUIManager::DirtyVariable(RmlModelHandle model, CONST String& name)
{
	auto* reg = m_model_registry;
	if (!reg) return;
	auto it = reg->find(model.value);
	if (it != reg->end())
		it->second.handle.DirtyVariable(name);
}

void RmlUIManager::RemoveDataModel(RmlModelHandle model)
{
	auto* reg = m_model_registry;
	if (!reg) return;
	auto it = reg->find(model.value);
	if (it != reg->end())
	{
		auto* ctx = m_context;
		if (ctx) ctx->RemoveDataModel(it->second.name);
		reg->erase(it);
	}
}

Rml::DataModelConstructor* RmlUIManager::GetModelConstructor(RmlModelHandle model)
{
	auto* reg = m_model_registry;
	if (!reg) return nullptr;
	auto it = reg->find(model.value);
	if (it != reg->end())
		return &it->second.ctor;
	return nullptr;
}

// -----------------------------------------------------------------------
// Document management
// -----------------------------------------------------------------------
RmlDocHandle RmlUIManager::LoadDocument(CONST String& path)
{
	auto* ctx = m_context;
	if (!ctx) return {};

	auto* doc = ctx->LoadDocument(path);
	if (!doc)
	{
		std::cerr << "[RmlUIManager] Failed to load document: " << path << std::endl;
		return {};
	}

	RmlDocHandle h; h.value = m_next_doc_handle++;
	auto* reg = m_doc_registry;
	(*reg)[h.value] = { doc };
	return h;
}

void RmlUIManager::ShowDocument(RmlDocHandle doc)
{
	auto* reg = m_doc_registry;
	if (!reg) return;
	auto it = reg->find(doc.value);
	if (it != reg->end() && it->second.doc)
		it->second.doc->Show();
}

void RmlUIManager::HideDocument(RmlDocHandle doc)
{
	auto* reg = m_doc_registry;
	if (!reg) return;
	auto it = reg->find(doc.value);
	if (it != reg->end() && it->second.doc)
		it->second.doc->Hide();
}

void RmlUIManager::CloseDocument(RmlDocHandle doc)
{
	auto* reg = m_doc_registry;
	if (!reg) return;
	auto it = reg->find(doc.value);
	if (it != reg->end() && it->second.doc)
		it->second.doc->Close();
	reg->erase(doc.value);
}

// -----------------------------------------------------------------------
// Accessors
// -----------------------------------------------------------------------
UI::UIRenderer* RmlUIManager::GetRenderer() CONST
{
	return m_renderer;
}

RmlUIInputBridge* RmlUIManager::GetInputBridge() CONST
{
	return m_input_bridge;
}

bool RmlUIManager::LoadFontFace(CONST String& file_path)
{
	return Rml::LoadFontFace(file_path);
}

bool RmlUIManager::IsMouseInteracting() CONST
{
	auto* ctx = m_context;
	if (!ctx) return false;
	return ctx->IsMouseInteracting();
}

void RmlUIManager::SetupInterfaces()
{
	m_system_interface = new RmlUISystemInterface();
	m_system_interface->Install();

	m_file_interface = new RmlUIFileInterface();
	m_file_interface->Install();
}

void RmlUIManager::TeardownInterfaces()
{
	if (m_system_interface)
	{
		m_system_interface->Uninstall();
		delete m_system_interface;
		m_system_interface = nullptr;
	}
	if (m_file_interface)
	{
		m_file_interface->Uninstall();
		delete m_file_interface;
		m_file_interface = nullptr;
	}
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
