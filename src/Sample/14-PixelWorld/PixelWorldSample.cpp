// Sample 14-PixelWorld: CPU/GPU pixel material world (Phase 1 + Phase 2).
//
// Demonstrates:
//   - PixelWorld CPU simulator (sand falls, water flows, fire spreads)
//   - GpuPixelWorld GPU simulator (SSBO authoritative, 6-dispatch chain)
//   - G key toggles between CPU/GPU backends (same IPixelWorldSimulator
//     interface, same ITerrainEditSink edit path)
//   - Camera2D pan/zoom with world-space brush (LMB sand / RMB water /
//     scroll brush radius / MMB pan)
//
// Threading: the simulation runs inside the RDG sim-pass execute lambda
// (render thread) - dispatch recording must not happen on the logic thread
// in ThreeThread mode. Edit events are enqueued on the logic thread and
// consumed by the sim pass (mutex-protected queue).

#include "Application/SampleApp.h"
#include "Application/CameraController2D.h"
#include "Application/Window.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderCommandList.h"
#include "Render/View/Camera2D.h"
#include "Input/InputSystem.h"
#include "Input/InputKeys.h"
#include "World/PixelWorld.h"
#include "World/GpuPixelWorld.h"
#include "World/PixelWorldRenderer.h"
#include "World/MaterialRegistry.h"
#include <iostream>
#include <cstdlib>
using namespace MXRender;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;
using namespace MXRender::World;

namespace
{
	struct SimPassData : public RenderGraphPassDataBase
	{
		VIRTUAL ~SimPassData() MYDEFAULT;
		VIRTUAL void METHOD(Release)() OVERRIDE {}
	};
}

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(PixelWorldSampleApp, public Application::SampleApp)
#pragma region METHOD
public:
	PixelWorldSampleApp() MYDEFAULT;
	VIRTUAL ~PixelWorldSampleApp() MYDEFAULT;

	VIRTUAL void OnInitScene() OVERRIDE FINAL;
	VIRTUAL void OnShutdownScene() OVERRIDE FINAL;
	VIRTUAL void OnUpdate(float dt) OVERRIDE FINAL;
protected:
	void METHOD(HandleBrushInput)();
	void METHOD(PaintBrush)(Int cx, Int cy, Float32 radius, UInt8 material);
	void METHOD(HandleBackendToggle)();

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	Render::Camera2D m_camera;
	Application::CameraController2D m_controller;
	PixelWorld m_cpu_world;
	GpuPixelWorld m_gpu_world;
	PixelWorldRenderer m_renderer;
	UInt8 m_brush_material = 0;
	Float32 m_brush_radius = 3.0f;
	Bool m_use_gpu = false;
	Bool m_g_key_prev = false;
	Float32 m_accumulator = 0.0f;
	UInt32 m_frame_count = 0;

	// Shared with the sim pass (render thread reads the count, logic thread
	// writes it): use atomic for pending ticks.
	std::atomic<UInt32> m_pending_ticks{ 0 };

private:
#pragma endregion
MYRENDERER_END_CLASS

void PixelWorldSampleApp::OnInitScene()
{
	std::cout << "[PixelWorld] pixel world sample started (G toggles CPU/GPU)" << std::endl;
	std::cout << "[PixelWorld] LMB=sand RMB=water MMB=pan scroll=zoom/brush" << std::endl;

	// Camera: view the whole world initially (192 cells tall).
	m_camera.SetViewport(GetViewportWidth(), GetViewportHeight());
	m_camera.SetOrthoSize(192.0f);
	m_camera.SetPosition(glm::vec2(128.0f, 96.0f));
	m_camera.UpdateMatrices();
	m_controller.Attach(GetPlatformWindow());
	m_controller.pan_button = MouseButton::Middle;

	m_brush_material = MaterialRegistry::GetSand();

	// GPU backend: create resources + SRBs now so the display binding can
	// reference state_a at init time (SRB binding must happen once, at init).
	m_gpu_world.Init();

	// Start on GPU backend when PIXELWORLD_GPU=1 (scripted verification);
	// otherwise start on CPU and toggle with G.
	m_use_gpu = std::getenv("PIXELWORLD_GPU") != nullptr;
	m_renderer.SetDisplaySource(m_use_gpu);
	std::cout << "[PixelWorld] initial backend: " << (m_use_gpu ? "GPU" : "CPU") << std::endl;

	// Renderer: buffers + palette (no display pass yet - sim pass must be
	// registered BEFORE display so the mirror update is visible same-frame).
	m_renderer.Init();
	// Pre-place a sand pile so the screen is not empty at startup. The same
	// edits go to BOTH backends so CPU and GPU mode both show content.
	for (Int i = 0; i < 40; ++i)
	{
		for (Int j = 0; j < 30; ++j)
		{
			EditEvent edit{ 128 + i - 20, 90 + j, MaterialRegistry::GetSand(), kEditOpWrite };
			m_cpu_world.ApplyEdit(edit);
			m_gpu_world.ApplyEdit(edit);
		}
	}
	m_renderer.UploadMirror(m_cpu_world);
	std::cout << "[PixelWorld] pre-placed 40x30 sand block at (108,90)" << std::endl;

	// Simulation pass: runs BEFORE the display pass (RDG executes in
	// registration order). TickFrame records dispatches on the render thread.
	auto* sim_pass = graph.AddRenderPass<SimPassData>("PixelWorldSim", &graph,
		RHIGetImmediateCommandList(),
	[&](SimPassData& data, RenderGraphPassBuilder& builder, CommandList* cmd)
	{
		// No declared RDG resources (all world buffers are retained/external).
	},
	[&](CONST SimPassData& data, CommandList* cmd)
	{
		UInt32 ticks = m_pending_ticks.exchange(0);
		IPixelWorldSimulator::SimFrameContext ctx;
		ctx.cmd = cmd;
		ctx.pending_ticks = ticks;
		ctx.frame_index = m_frame_count++;
		ctx.tick_count = (UInt32)(m_use_gpu ? m_gpu_world.GetTickCount() : m_cpu_world.GetTickCount());

		if (m_use_gpu)
			m_gpu_world.TickFrame(ctx);
		else
		{
			m_cpu_world.TickFrame(ctx);
			m_renderer.UploadMirror(m_cpu_world);
		}
	});
	sim_pass->SetIsCullable(false);
	sim_pass->SetShaderPath("Shader/world_apply_edits");

	// Display pass: registered AFTER sim (executes after it each frame).
	m_renderer.RegisterDisplayPass(&graph, GetBackBufferResource(), GetDepthStencil(),
		RHIGetImmediateCommandList());
	m_renderer.CreateDisplayBindings(m_gpu_world.GetStateBuffer());
}

void PixelWorldSampleApp::OnShutdownScene()
{
	m_renderer.Shutdown();
}

void PixelWorldSampleApp::PaintBrush(Int cx, Int cy, Float32 radius, UInt8 material)
{
	// CPU-side circle fill: iterate the bounding box, keep cells inside the
	// circle. Deterministic order for reproducibility.
	Int r = (Int)radius;
	for (Int dy = -r; dy <= r; ++dy)
	{
		for (Int dx = -r; dx <= r; ++dx)
		{
			Float32 dist = std::sqrt((Float32)(dx * dx + dy * dy));
			if (dist > radius)
				continue;
			Int x = cx + dx;
			Int y = cy + dy;
			if (x < 0 || y < 0 || x >= (Int)World::kWorldW || y >= (Int)World::kWorldH)
				continue;
			EditEvent edit{ x, y, material, kEditOpWrite };
			// Route through the active backend's edit sink. CPU applies
			// immediately; GPU enqueues into the mutex-protected queue.
			if (m_use_gpu)
				m_gpu_world.ApplyEdit(edit);
			else
				m_cpu_world.ApplyEdit(edit);
		}
	}
}

void PixelWorldSampleApp::HandleBackendToggle()
{
	auto& input = MXRender::Input::InputSystem::Get();
	Bool g_down = input.IsKeyDown(MXRender::Input::EKey::G);
	if (g_down && !m_g_key_prev)
	{
		m_use_gpu = !m_use_gpu;
		m_renderer.SetDisplaySource(m_use_gpu);
		std::cout << "[PixelWorld] backend: " << (m_use_gpu ? "GPU (6-dispatch chain)" : "CPU") << std::endl;
	}
	m_g_key_prev = g_down;
}

void PixelWorldSampleApp::HandleBrushInput()
{
	auto& input = MXRender::Input::InputSystem::Get();

	// Brush radius via scroll (up = bigger).
	Float32 scroll = input.GetScrollDelta();
	if (scroll != 0.0f)
	{
		m_brush_radius = glm::clamp(m_brush_radius + scroll * 1.5f, 1.0f, 24.0f);
		std::cout << "[PixelWorld] brush radius = " << m_brush_radius << std::endl;
	}

	// Material selection: LMB sand, RMB water.
	Bool lmb = input.IsMouseDown((Int)MouseButton::Left);
	Bool rmb = input.IsMouseDown((Int)MouseButton::Right);
	UInt8 material = lmb ? MaterialRegistry::GetSand() : (rmb ? MaterialRegistry::GetWater() : m_brush_material);

	Float32 mx = 0.0f, my = 0.0f;
	input.GetMousePos(mx, my);
	glm::vec2 world_pos = m_camera.ScreenToWorld(mx, my);
	glm::ivec2 cell = m_cpu_world.CellFromWorld(world_pos);   // 1 cell = 1 unit, backend-independent

	if (lmb || rmb)
		PaintBrush(cell.x, cell.y, m_brush_radius, material);
}

void PixelWorldSampleApp::OnUpdate(float dt)
{
	m_controller.Update(dt, m_camera);
	HandleBackendToggle();
	HandleBrushInput();

	// Fixed 60 Hz tick accumulator (max 4 catch-up ticks per frame).
	// The pending count is consumed by the sim pass on the render thread.
	m_accumulator += dt;
	constexpr Float32 kTickRate = 1.0f / 60.0f;
	UInt32 ticks = 0;
	while (m_accumulator >= kTickRate && ticks < 4)
	{
		m_accumulator -= kTickRate;
		++ticks;
	}
	m_pending_ticks.store(ticks);

	// Periodic status line.
	UInt64 total_ticks = m_use_gpu ? m_gpu_world.GetTickCount() : m_cpu_world.GetTickCount();
	if (total_ticks % 120 == 0 && total_ticks > 0)
	{
		std::cout << "[PixelWorld] tick=" << total_ticks << " backend="
			<< (m_use_gpu ? "gpu" : "cpu") << std::endl;
	}
}

int main()
{
	PixelWorldSampleApp app;
	return SampleApp::RunSample(app, "MXRender PixelWorld");
}
