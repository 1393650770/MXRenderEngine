#include "RmlUISystem.h"
#include "RmlUISystemInterface.h"
#include "RmlUIFileInterface.h"
#include "RmlUIInputBridge.h"
#include "RmlUIRenderer.h"
#include "RmlUIRenderInterface.h"
#include "RmlDataModelBinder.h"
#include "RmlUIHotReloadService.h"
#include "UI/UIRenderer.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Debugger.h>

#include "RHI/RenderViewport.h"
#include "RHI/RenderTexture.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderCommandList.h"
#include "Render/Core/CommandQueue.h"
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

	// Hot-reload file watcher (logic-thread owned)
	m_hot_reload_service = new RmlUIHotReloadService();

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
	// The debugger menu is visible by default — start hidden, F8 reveals it.
	if (m_context)
		Rml::Debugger::SetVisible(false);

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

	// 7. Release hot-reload service
	delete m_hot_reload_service; m_hot_reload_service = nullptr;

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

	// Sync the output copy from the source BEFORE loading (xmake's incremental
	// resource copy is unreliable; a stale output file must not be loaded).
	if (m_hot_reload_service)
		m_hot_reload_service->TrackPath(path);

	auto* doc = ctx->LoadDocument(path);
	if (!doc)
	{
		std::cerr << "[RmlUISystem] Failed to load document: " << path << std::endl;
		return {};
	}

	UIDocHandle h; h.value = m_next_doc_handle++;
	auto* reg = m_doc_registry;
	(*reg)[h.value] = DocEntry{ doc, path };
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
// Dev tooling: hot reload + debugger
//
// Thread model (matches the whole RmlUI backend):
//   - PollHotReload runs on the LOGIC thread (cheap mtime stats, driven by
//     UIManager::Update); it only builds a UIHotReloadChangeSet.
//   - The change set is passed BY VALUE into a render-thread command;
//     ApplyHotReload mutates the doc registry / Rml::Context on the RENDER
//     thread (their owning thread).
// Known limitation: the RmlUi debugger's console cannot receive typed text —
// there is no char-callback source in the platform input layer yet.
// =========================================================================

void RmlUISystem::EnableHotReload(bool enabled)
{
	if (m_hot_reload_service)
		m_hot_reload_service->Enable(enabled);
}

void RmlUISystem::PollHotReload(Float32 dt)
{
	ASSERT_LOGIC_THREAD();
	if (!m_hot_reload_service) return;

	UIHotReloadChangeSet changes = m_hot_reload_service->Poll(dt);
	if (changes.empty()) return;

	// Apply on the render thread (context + doc registry are render-thread owned).
	ENQUEUE_RENDER_COMMAND(UIHotReloadApply)([this, changes]()
	{
		ApplyHotReload(changes);
	});
}

void RmlUISystem::ApplyHotReload(const UIHotReloadChangeSet& changes)
{
	ASSERT_RENDER_THREAD();
	auto* ctx = m_context;
	if (!ctx || !m_doc_registry) return;

	if (changes.rcss_changed)
	{
		// Reload every open document's stylesheet. RmlUi clears the stylesheet
		// cache internally, so documents sharing one .rcss all refresh without
		// us parsing <link> references. Inline `style` attributes are NOT
		// re-read (documented RmlUi limitation).
		Int count = 0;
		for (auto& entry : *m_doc_registry)
			if (entry.second.doc) { entry.second.doc->ReloadStyleSheet(); ++count; }
		std::cout << "[UIHotReload] stylesheet reloaded for " << count << " doc(s)" << std::endl;
	}

	for (const auto& path : changes.rml_changed_paths)
	{
		// Swap mode: load the NEW document first; only on success close the old
		// one and swap the pointer in-place (UIDocHandle stays valid, the
		// data-model rebinds automatically via the `data-model` attribute).
		// On failure keep the old document — a broken edit must not destroy UI.
		// Note: RmlUi's XML parser is TOLERANT — it logs warnings and still
		// returns a document for malformed input, so we capture ERROR/WARNING
		// log messages during LoadDocument and treat any as a failed load.
		Int matches = 0;
		Int aborted = 0;
		for (auto& entry : *m_doc_registry)
		{
			if (entry.second.path != path || !entry.second.doc) continue;
			if (m_system_interface)
				m_system_interface->BeginLogCapture();
			auto* new_doc = ctx->LoadDocument(path);
			const Int load_problems = m_system_interface ? m_system_interface->EndLogCapture() : 0;
			if (!new_doc || load_problems > 0)
			{
				if (new_doc) new_doc->Close(); // discard the mangled fresh copy
				std::cerr << "[UIHotReload] reload aborted: " << path << " — "
					<< load_problems << " parse warning(s)/error(s), keeping old document" << std::endl;
				++aborted;
				continue;
			}
			auto* old_doc = entry.second.doc;
			entry.second.doc = new_doc;
			if (old_doc) old_doc->Close();
			new_doc->Show();   // dev tool: always show the fresh copy
			++matches;
		}
		if (matches > 0)
			std::cout << "[UIHotReload] RML reloaded: " << path << " (" << matches << " doc(s))" << std::endl;
		else if (aborted == 0)
			std::cout << "[UIHotReload] RML changed, no matching open doc: " << path << std::endl;
	}
}

void RmlUISystem::ToggleDebugger()
{
	// Debugger plugin was initialised in Init(); SetVisible/IsVisible are
	// null-pointer safe. Dispatch to the render thread like everything else
	// that touches Rml::Context.
	ENQUEUE_RENDER_COMMAND(UIToggleDebugger)([this]()
	{
		ASSERT_RENDER_THREAD();
		if (m_context)
		{
			// Note: Debugger::IsVisible() reads the per-frame computed flag
			// (updated during ctx->Update), so log the INTENDED state, not
			// the queried one (which lags one frame).
			const bool visible = Rml::Debugger::IsVisible();
			Rml::Debugger::SetVisible(!visible);
			std::cout << "[UIHotReload] debugger " << (!visible ? "shown" : "hidden") << std::endl;
		}
	});
}

void RmlUISystem::ReloadAllDocuments()
{
	// Build the change set INSIDE the command (doc registry is render-thread
	// owned — never read it from the logic thread).
	ENQUEUE_RENDER_COMMAND(UIHotReloadApply)([this]()
	{
		UIHotReloadChangeSet changes;
		changes.rcss_changed = true;
		if (m_doc_registry)
			for (auto& entry : *m_doc_registry)
				changes.rml_changed_paths.push_back(entry.second.path);
		ApplyHotReload(changes);
	});
}

// =========================================================================
// Editor designer support (WYSIWYG overlay)
// =========================================================================

String RmlUISystem::PickElementAt(Int x, Int y)
{
	auto* ctx = m_context;
	if (!ctx) return {};
	::Rml::Element* el = ctx->GetElementAtPoint(::Rml::Vector2f((float)x, (float)y));
	for (; el; el = el->GetParentNode())
	{
		const ::Rml::String& id = el->GetId();
		if (!id.empty())
			return String(id.c_str());
	}
	return {};
}

namespace
{
/// Context-wide element lookup: the context root wraps every open document.
::Rml::Element* FindElementById(::Rml::Context* ctx, const String& id)
{
	if (!ctx) return nullptr;
	::Rml::Element* root = ctx->GetRootElement();
	return root ? root->GetElementById(id) : nullptr;
}
} // namespace

Bool RmlUISystem::GetElementBox(const String& id, Float32& x, Float32& y,
	Float32& w, Float32& h)
{
	auto* ctx = m_context;
	if (!ctx) return false;
	::Rml::Element* el = FindElementById(ctx, id);
	if (!el) return false;
	x = el->GetAbsoluteLeft();
	y = el->GetAbsoluteTop();
	::Rml::Vector2f size = el->GetBox().GetSize();
	w = size.x;
	h = size.y;
	return true;
}

void RmlUISystem::SetElementBoxTransient(const String& id, Float32 x, Float32 y,
	Float32 w, Float32 h)
{
	auto* ctx = m_context;
	if (!ctx) return;
	::Rml::Element* el = FindElementById(ctx, id);
	if (!el) return;
	// Inline style overlay — NOT re-read by hot reload, so drag previews
	// vanish on reload. The designer commits boxes into #id rules instead.
	el->SetProperty("position", ::Rml::String("absolute"));
	el->SetProperty("left", ::Rml::String(std::to_string((int)x) + "px"));
	el->SetProperty("top", ::Rml::String(std::to_string((int)y) + "px"));
	if (w > 0.0f)
		el->SetProperty("width", ::Rml::String(std::to_string((int)w) + "px"));
	if (h > 0.0f)
		el->SetProperty("height", ::Rml::String(std::to_string((int)h) + "px"));
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
