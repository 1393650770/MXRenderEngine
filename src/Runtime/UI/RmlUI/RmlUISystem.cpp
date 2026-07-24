#include "RmlUISystem.h"
#include "RmlUISystemInterface.h"
#include "RmlUIFileInterface.h"
#include "RmlUIInputBridge.h"
#include "RmlUIRenderer.h"
#include "RmlUIRenderInterface.h"
#include "RmlDataModelBinder.h"
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

// =========================================================================
// Lifecycle
// =========================================================================

RmlUISystem::RmlUISystem()
{
}

RmlUISystem::~RmlUISystem()
{
	if (m_initialized)
		Shutdown();
}

void RmlUISystem::Init(RHI::Viewport* viewport)
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
		std::cerr << "[RmlUISystem] Failed to create RmlUI context!" << std::endl;
		return;
	}

	// Enable RmlUi debugger (F8 to toggle)
	if (m_context && !Rml::Debugger::IsVisible())
		Rml::Debugger::Initialise(m_context);

	auto* ctx = m_context;
	std::cout << "[RmlUISystem] Initialized — context \"" << ctx->GetName()
		<< "\" " << dims.x << "x" << dims.y << std::endl;

	m_input_bridge = new RmlUIInputBridge();
	m_input_bridge->SetContext(ctx);

	// Initialize renderer GPU resources (shaders, PSOs)
	auto* cmd = RHIGetImmediateCommandList();
	auto* bb_rtv = viewport->GetCurrentBackBufferRTV();
	auto* bb_dsv = viewport->GetCurrentBackBufferDSV();
	m_renderer->Initialize(cmd, bb_rtv, bb_dsv, size[0], size[1]);
	m_renderer->SetViewport(viewport);

	m_initialized = true;
}

void RmlUISystem::Shutdown()
{
	if (!m_initialized) return;

	// 1. Rml::Shutdown() — calls back render interface's ReleaseGeometry/ReleaseTexture
	Rml::Shutdown();

	// 2. Release render interface
	delete m_render_interface; m_render_interface = nullptr;

	// 3. Release renderer (GPU resources)
	delete m_renderer; m_renderer = nullptr;

	// 4. Release input bridge
	delete m_input_bridge; m_input_bridge = nullptr;

	// 5. Teardown system/file interfaces
	TeardownInterfaces();

	// 6. Delete registries
	delete m_model_registry; m_model_registry = nullptr;
	delete m_doc_registry;   m_doc_registry = nullptr;

	m_initialized = false;
	m_context = nullptr;

	std::cout << "[RmlUISystem] Shutdown complete." << std::endl;
}

// =========================================================================
// Per-frame
// =========================================================================

void RmlUISystem::Update(Float32 dt)
{
	m_elapsed_time += dt;

	// Propagate accumulated time to SystemInterface
	if (m_system_interface)
		m_system_interface->SetElapsedTime(m_elapsed_time);

	auto* ctx = m_context;
	if (!ctx) return;

	auto size = m_viewport->GetViewportSize();
	ctx->SetDimensions(Rml::Vector2i(static_cast<int>(size[0]), static_cast<int>(size[1])));
	ctx->Update();
}

void RmlUISystem::Render(RHI::CommandList* cmd)
{
	auto* ctx = m_context;
	if (!ctx || !m_renderer) return;

	m_renderer->BeginFrame(cmd);
	ctx->Render();
	m_renderer->EndFrame(cmd);
}

// =========================================================================
// Sub-system access
// =========================================================================

UIRenderer* RmlUISystem::GetRenderer() CONST
{
	return m_renderer;
}

UIInputBridge* RmlUISystem::GetInputBridge() CONST
{
	return m_input_bridge;
}

// =========================================================================
// Data model
// =========================================================================

UIModelHandle RmlUISystem::CreateDataModel(CONST String& name)
{
	auto* ctx = m_context;
	if (!ctx) return {};

	auto ctor = ctx->CreateDataModel(name, nullptr, false);
	if (!ctor) return {};

	UIModelHandle h; h.value = m_next_model_handle++;
	auto* reg = m_model_registry;
	ModelEntry entry;
	entry.name = name;
	entry.ctor = ctor;
	entry.handle = ctor.GetModelHandle();
	(*reg)[h.value] = entry;
	return h;
}

void RmlUISystem::DirtyVariable(UIModelHandle model, CONST String& name)
{
	auto* reg = m_model_registry;
	if (!reg) return;
	auto it = reg->find(model.value);
	if (it != reg->end())
		it->second.handle.DirtyVariable(name);
}

void RmlUISystem::RemoveDataModel(UIModelHandle model)
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

Rml::DataModelConstructor* RmlUISystem::GetModelConstructor(UIModelHandle model)
{
	auto* reg = m_model_registry;
	if (!reg) return nullptr;
	auto it = reg->find(model.value);
	if (it != reg->end())
		return &it->second.ctor;
	return nullptr;
}

UIDataModelBinder* RmlUISystem::GetModelBinder(UIModelHandle model)
{
	auto* ctor = GetModelConstructor(model);
	if (!ctor) return nullptr;

	m_cached_binder = std::make_unique<RmlDataModelBinder>(ctor);
	return m_cached_binder.get();
}

void RmlUISystem::BindEventCallback(UIModelHandle model, CONST String& name,
	std::function<void()> callback)
{
	auto* ctor = GetModelConstructor(model);
	if (!ctor) return;

	// Wrap plain callback into Rml::DataEventFunc, stripping all Rml params
	ctor->BindEventCallback(name,
		[cb = std::move(callback)](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
			cb();
		});
}

// =========================================================================
// Document / Panel
// =========================================================================

UIDocHandle RmlUISystem::LoadPanel(CONST String& path)
{
	auto* ctx = m_context;
	if (!ctx) return {};

	auto* doc = ctx->LoadDocument(path);
	if (!doc)
	{
		std::cerr << "[RmlUISystem] Failed to load document: " << path << std::endl;
		return {};
	}

	UIDocHandle h; h.value = m_next_doc_handle++;
	auto* reg = m_doc_registry;
	(*reg)[h.value] = { doc };
	return h;
}

void RmlUISystem::ShowPanel(UIDocHandle doc)
{
	auto* reg = m_doc_registry;
	if (!reg) return;
	auto it = reg->find(doc.value);
	if (it != reg->end() && it->second.doc)
		it->second.doc->Show();
}

void RmlUISystem::HidePanel(UIDocHandle doc)
{
	auto* reg = m_doc_registry;
	if (!reg) return;
	auto it = reg->find(doc.value);
	if (it != reg->end() && it->second.doc)
		it->second.doc->Hide();
}

void RmlUISystem::ClosePanel(UIDocHandle doc)
{
	auto* reg = m_doc_registry;
	if (!reg) return;
	auto it = reg->find(doc.value);
	if (it != reg->end() && it->second.doc)
		it->second.doc->Close();
	reg->erase(doc.value);
}

// =========================================================================
// Resources
// =========================================================================

bool RmlUISystem::LoadFont(CONST String& path)
{
	return Rml::LoadFontFace(path);
}

bool RmlUISystem::IsMouseInteracting() CONST
{
	auto* ctx = m_context;
	if (!ctx) return false;
	return ctx->IsMouseInteracting();
}

// =========================================================================
// Internal: interfaces
// =========================================================================

void RmlUISystem::SetupInterfaces()
{
	m_system_interface = new RmlUISystemInterface();
	m_system_interface->Install();

	m_file_interface = new RmlUIFileInterface();
	m_file_interface->Install();
}

void RmlUISystem::TeardownInterfaces()
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

MYRENDERER_END_NAMESPACE // RmlUI
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
