#include "SampleApp.h"
#include <iostream>
#include <cstdlib>
#include "Platform/PlatformWindow.h"
#include "Application/Window.h"
#include "RHI/RenderViewport.h"
#include "RHI/RenderTexture.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderBuffer.h"
#include "Render/Core/RenderGraphDefinition.h"
#include "Render/Core/RenderGraphSerializer.h"
#include "Network/NetworkManager.h"
#if PLATFORM_WIN32 || PLATFORM_LINUX
#include "Platform/Desktop/DesktopNetwork.h"
#elif PLATFORM_GLES3
#include "Platform/MiniGame/MiniGameNetwork.h"
#endif

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)

void* SampleApp::s_platform_data = nullptr;

void SampleApp::SetPlatformData(void* data)
{
	s_platform_data = data;
}

Int SampleApp::RunSample(SampleApp& in_app, CONST String& in_title)
{
	Window window(in_title, 1280, 960, s_platform_data);
	window.InitWindow();
	window.Run(&in_app);
#if PLATFORM_WIN32
	system("pause");
#endif
	return 0;
}

void SampleApp::OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport)
{
	platform_window = in_window;
	viewport = in_viewport;

	RHI::Texture* backbuffer_rtv = viewport->GetCurrentBackBufferRTV();
	RHI::Texture* backbuffer_dsv = viewport->GetCurrentBackBufferDSV();

	backbuffer_resource = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
		"BackBuffer", backbuffer_rtv->GetTextureDesc(), backbuffer_rtv);
	if (backbuffer_dsv)
	{
		depth_stencil_resource = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
			"DepthStencil", backbuffer_dsv->GetTextureDesc(), backbuffer_dsv);
	}

	// Wire up network subsystem (platform-specific backend)
#if PLATFORM_WIN32 || PLATFORM_LINUX
	Network::NetworkManager::Create(new Network::Desktop::DesktopNetwork());
#elif PLATFORM_GLES3
	Network::NetworkManager::Create(new Network::MiniGame::MiniGameNetwork());
#endif

	OnInitScene();

	if (auto_compile)
		graph.Compile();
}

void SampleApp::OnShutdown_Logic()
{
	OnShutdownScene();
	Network::NetworkManager::Destroy();
	graph.Release();
}

void SampleApp::OnRender()
{
	// Update retained texture pointers after potential swapchain recreation (UE: like EditorRender)
	if (backbuffer_resource)
		backbuffer_resource->UpdateRetainedPtr(GetBackBuffer());
	if (depth_stencil_resource)
		depth_stencil_resource->UpdateRetainedPtr(GetDepthStencil());
	graph.Execute();
}

RHI::Texture* SampleApp::GetBackBuffer() CONST
{
	return viewport->GetCurrentBackBufferRTV();
}

RHI::Texture* SampleApp::GetDepthStencil() CONST
{
	return viewport->GetCurrentBackBufferDSV();
}

UInt32 SampleApp::GetViewportWidth() CONST
{
	return viewport ? viewport->GetViewportSizeWidth() : 1280;
}

UInt32 SampleApp::GetViewportHeight() CONST
{
	return viewport ? viewport->GetViewportSizeHeight() : 960;
}

void SampleApp::BindBackBufferTarget(RHI::CommandList* in_cmd) CONST
{
	Vector<RHI::Texture*> rtvs = { GetBackBuffer() };
	RHI::Texture* dsv = GetDepthStencil();
	Vector<RHI::ClearValue> clear_values;
	for (auto* rtv : rtvs)
		clear_values.push_back(rtv->GetTextureDesc().clear_value);
	if (dsv)
		clear_values.push_back(dsv->GetTextureDesc().clear_value);
	in_cmd->SetRenderTarget(rtvs, dsv, clear_values, dsv != nullptr);
}

void SampleApp::SaveGraphDefinition(CONST String& in_graph_name, CONST String& in_file_path)
{
	Render::RenderGraphDefinition def;
	def.graph_name = in_graph_name;
	def.version = 2;

	for (auto& res : graph.GetResources())
	{
		Render::RDGResourceDef rd;
		rd.name = res->GetName();
		rd.is_transient = res->GetIsTransient();
		if (auto* tex = res->GetAsTexture())
		{
			rd.desc = tex->GetTextureDesc();
		}
		else if (res->GetAsBuffer())
		{
			rd.desc = res->GetAsBuffer()->GetBufferDesc();
		}
		def.resources.push_back(rd);
	}

	for (auto& pass : graph.GetPasses())
	{
		Render::RDGPassDef pd;
		pd.name = pass->GetName();
		pd.pass_kind = Render::RDGPassKind::Graphics;
		pd.shader_path = pass->GetShaderPath();
		pd.vertex_count = pass->GetVertexCount();
		for (auto* r : pass->GetReadResources())  pd.read_resources.push_back(r->GetName());
		for (auto* w : pass->GetWriteResources()) pd.write_resources.push_back(w->GetName());
		for (auto* c : pass->GetCreateResources()) pd.create_resources.push_back(c->GetName());
		def.passes.push_back(pd);
	}

	Render::RenderGraphSerializer::SaveGraph(def, in_file_path);
	std::cout << "[SampleApp] Graph saved to " << in_file_path << std::endl;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
