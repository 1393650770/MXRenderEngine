// Sample 13-2D: 2D game rendering with Camera2D + AudioManager.
//
// Demonstrates:
//   - Camera2D orthographic projection + CameraController2D pan/zoom
//   - AudioManager: PlayBGM at startup + mouse click triggers SFX
//   - Simple quad-batch sprite rendering (full quad rebuilt per frame for simplicity)
//   - A colorful cross of quads in world space (draggable, zoomable via scroll)

#include "Application/SampleApp.h"
#include "Application/CameraController2D.h"
#include "Application/Window.h"
#include "Audio/AudioManager.h"
#include "Audio/Desktop/MiniAudioEngine.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/RenderBuffer.h"
#include "RHI/RenderTexture.h"
#include "Tool/ShaderLibrary.h"
#include "Tool/BufferUtils.h"
#include "Render/View/Camera2D.h"
#include "Input/InputSystem.h"
#include "Input/InputKeys.h"
#include <iostream>
#include <cmath>
using namespace MXRender;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;
using namespace MXRender::Audio;

// ---- Quad vertex with position + UV (interleaved) ----
struct QuadVertex { float x, y, u, v; };

// ---- Shader uniform block (matches sprite2d.vert binding 0) ----
struct SpriteParams { glm::mat4 mvp; };

// ---- Pass data (created once at init) ----
struct SpritePassData : public RenderGraphPassDataBase
{
	RenderPipelineState* pso = nullptr;
	ShaderResourceBinding* srb = nullptr;
	VIRTUAL ~SpritePassData() MYDEFAULT;
	VIRTUAL void Release() OVERRIDE { delete srb; srb = nullptr; }
};

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(Sample2DApp, public Application::SampleApp)
#pragma region METHOD
public:
	Sample2DApp() MYDEFAULT;
	VIRTUAL ~Sample2DApp() MYDEFAULT;
	VIRTUAL void OnInitScene() OVERRIDE FINAL;
	VIRTUAL void OnShutdownScene() OVERRIDE FINAL;
	VIRTUAL void OnUpdate(float dt) OVERRIDE FINAL;
protected:
private:
	void PlayStartupBGM();
	void HandleMouseClick();
	void RebuildQuads(QuadVertex* out, UInt32& out_count) CONST;
#pragma endregion

#pragma region MEMBER
public:
protected:
	Render::Camera2D m_camera;
	Application::CameraController2D m_controller;
	RHI::Buffer* m_vb = nullptr;
	RHI::Buffer* m_ib = nullptr;
	RHI::Buffer* m_param_buf = nullptr;
	SpriteParams m_cached_params{};
	AudioClipHandle m_loaded_clip{};
	Bool m_mouse_was_down = false;
	static constexpr UInt32 MAX_QUADS = 64;
private:
#pragma endregion
MYRENDERER_END_CLASS

void Sample2DApp::OnInitScene()
{
	std::cout << "[2D Sample] Starting 2D Demo with Camera2D + Audio" << std::endl;

	// ---- Audio ----
	auto* audio = new Audio::Desktop::MiniAudioEngine();
	AudioManager::Create(audio);
	std::cout << "[2D Sample] AudioManager created. MMB drag=pan, scroll=zoom, LMB click=SFX" << std::endl;

	// ---- Camera ----
	m_camera.SetViewport(GetViewportWidth(), GetViewportHeight());
	m_camera.SetOrthoSize(20.0f);
	m_camera.SetPosition(glm::vec2(0.0f, 0.0f));
	m_camera.UpdateMatrices();
	m_controller.Attach(GetPlatformWindow());
	m_controller.zoom_step = 0.9f;
	std::cout << "[2D Sample] Camera2D initialized (ortho 20 world units tall, MMB pan, scroll zoom)" << std::endl;

	// ---- Vertex buffer (dynamic, rebuilt per frame) ----
	RHI::BufferDesc vb_desc;
	vb_desc.type = ENUM_BUFFER_TYPE::Vertex | ENUM_BUFFER_TYPE::Dynamic;
	vb_desc.size = sizeof(QuadVertex) * MAX_QUADS * 4;
	vb_desc.stride = sizeof(QuadVertex);
	m_vb = g_render_rhi->CreateBuffer(vb_desc);

	// ---- Index buffer (static: 6 indices per quad, 0-1-2 + 2-3-0) ----
	Vector<UInt16> indices;
	for (UInt32 i = 0; i < MAX_QUADS; ++i)
	{
		UInt16 base = (UInt16)(i * 4);
		indices.insert(indices.end(), { base, (UInt16)(base+1), (UInt16)(base+2),
		                                (UInt16)(base+2), (UInt16)(base+3), (UInt16)(base) });
	}
	RHI::BufferDesc ib_desc;
	ib_desc.type = ENUM_BUFFER_TYPE::Index | ENUM_BUFFER_TYPE::Dynamic;
	ib_desc.size = (UInt32)(indices.size() * sizeof(UInt16));
	ib_desc.stride = sizeof(UInt16);
	m_ib = g_render_rhi->CreateBuffer(ib_desc);
	Tool::BufferUtils::Upload(m_ib, indices.data(), ib_desc.size);

	m_param_buf = Tool::BufferUtils::CreateDynamicParamBuffer(sizeof(SpriteParams));

	// ---- RenderGraph pass ----
	auto* pass = graph.AddRenderPass<SpritePassData>("Sprite2DPass", &graph, RHIGetImmediateCommandList(),
	[&](SpritePassData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd)
	{
		builder.Write(GetBackBufferResource());

		Shader* vs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Vertex,
			"Shader/Sample/2D/sprite2d.vert.spv");
		Shader* ps = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Pixel,
			"Shader/Sample/2D/sprite2d.frag.spv");

		RenderGraphiPipelineStateDesc pd{};
		pd.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
		pd.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps;
		pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		pd.render_targets = { GetBackBuffer() };
		pd.raster_state.sample_count = 1;
		pd.raster_state.cull_mode = ENUM_RASTER_CULLMODE::None;
		pd.blend_state.render_targets.resize(1);

		// Bindless / inline: vertex attributes from the interleaved buffer
		VertexInputLayout pos_layout;
		pos_layout.binding = 0;
		pos_layout.location = 0;
		pos_layout.attribute_format = ENUM_TEXTURE_FORMAT::RG32F;
		pos_layout.input_rate = ENUM_VERTEX_INPUTRATE::PerVertex;
		pos_layout.offset = 0;

		VertexInputLayout uv_layout;
		uv_layout.binding = 0;
		uv_layout.location = 1;
		uv_layout.attribute_format = ENUM_TEXTURE_FORMAT::RG32F;
		uv_layout.input_rate = ENUM_VERTEX_INPUTRATE::PerVertex;
		uv_layout.offset = (UInt32)(sizeof(float) * 2);

		pd.vertex_input_layout.push_back(pos_layout);
		pd.vertex_input_layout.push_back(uv_layout);

		data.pso = g_render_rhi->CreateRenderPipelineState(pd);
		data.pso->CreateShaderResourceBinding(data.srb, false);
		data.srb->SetResource("params", m_param_buf);
		data.srb->FlushDescriptorWrites();
		delete vs;
		delete ps;
	},
	[=](CONST SpritePassData& data, CommandList* in_cmd)
	{
		BindBackBufferTarget(in_cmd);

		QuadVertex quads[MAX_QUADS * 4];
		UInt32 total_quads = 0;
		RebuildQuads(quads, total_quads);

		if (total_quads > 0)
		{
			UInt32 vert_bytes = total_quads * 4 * sizeof(QuadVertex);
			UInt32 idx_count = total_quads * 6;
			Tool::BufferUtils::Upload(m_vb, quads, vert_bytes);
			m_cached_params.mvp = m_camera.GetViewProjectionMatrix();
			Tool::BufferUtils::Upload(m_param_buf, &m_cached_params, sizeof(SpriteParams));

			in_cmd->SetGraphicsPipeline(data.pso);
			in_cmd->SetShaderResourceBinding(data.srb);
			in_cmd->SetVertexBuffer(m_vb, 0, sizeof(QuadVertex), 0);
			in_cmd->SetIndexBuffer(m_ib, 0, true);
			in_cmd->DrawIndexed(idx_count, 1, 0, 0, 0);
		}
	});
	pass->SetIsCullable(false);
	pass->SetShaderPath("Shader/Sample/2D/sprite2d");

	// Start BGM after a short delay (audio files are optional - fails silently)
	PlayStartupBGM();
}

void Sample2DApp::PlayStartupBGM()
{
	// BGM file is optional; miniaudio engine will silently ignore missing files.
	AudioManager::Get().PlayBGM("ArcadeBGM.wav", true);
	std::cout << "[2D Sample] Requested BGM: ArcadeBGM.wav" << std::endl;
}

void Sample2DApp::HandleMouseClick()
{
	auto& input = MXRender::Input::InputSystem::Get();
	Bool mouse_down = input.IsMouseDown((Int)MouseButton::Left);
	if (mouse_down && !m_mouse_was_down)
	{
		// Play SFX on each new click
		AudioManager::Get().PlaySFX("Blip.wav");
		Float32 mx, my;
		input.GetMousePos(mx, my);
		glm::vec2 world_pos = m_camera.ScreenToWorld(mx, my);
		std::cout << "[2D Sample] Click at world (" << world_pos.x << ", "
			<< world_pos.y << ") + SFX" << std::endl;
	}
	m_mouse_was_down = mouse_down;
}

void Sample2DApp::RebuildQuads(QuadVertex* out, UInt32& out_count) CONST
{
	// 5 overlapping colored quads in a cross pattern (world-space coordinates)
	Float32 s = 3.0f; // half-size
	QuadVertex quads[] = {
		// Red center
		{ -s, -s, 0,0 }, { s, -s, 1,0 }, { s, s, 1,1 }, { -s, s, 0,1 },
		// Green top-left
		{ -10, 2, 0,0 }, { -6, 2, 1,0 }, { -6, 6, 1,1 }, { -10, 6, 0,1 },
		// Blue top-right
		{ 6, 2, 0,0 }, { 10, 2, 1,0 }, { 10, 6, 1,1 }, { 6, 6, 0,1 },
		// Yellow bottom-left
		{ -10, -6, 0,0 }, { -6, -6, 1,0 }, { -6, -2, 1,1 }, { -10, -2, 0,1 },
		// Magenta bottom-right
		{ 6, -6, 0,0 }, { 10, -6, 1,0 }, { 10, -2, 1,1 }, { 6, -2, 0,1 },
	};
	out_count = 5;
	for (UInt32 i = 0; i < out_count * 4; ++i) out[i] = quads[i];
}

void Sample2DApp::OnShutdownScene()
{
	if (m_loaded_clip.IsValid())
		AudioManager::Get().UnloadClip(m_loaded_clip);
	AudioManager::Destroy();

	delete m_vb; m_vb = nullptr;
	delete m_ib; m_ib = nullptr;
	delete m_param_buf; m_param_buf = nullptr;
}

void Sample2DApp::OnUpdate(float dt)
{
	m_controller.Update(dt, m_camera);
	HandleMouseClick();
}

int main()
{
	Sample2DApp app;
	return SampleApp::RunSample(app, "MXRender 2D Demo");
}
