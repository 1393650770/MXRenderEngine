#include "Render/Core/RenderGraphSerializer.h"
#include<iostream>
#include <fstream>
#include "Platform/PlatformWindow.h"
#include "Application/Window.h"
#include "Render/RenderInterface.h"
#include "Render/Core/RenderGraph.h"
#include "RHI/RenderResource.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderCommandList.h"
#include "RHI/Vulkan/VK_Viewport.h"
#include "RHI/RenderTexture.h"
#include "RHI/RenderShader.h"
#include "RHI/RenderPipelineState.h"
#include "Asset/TextureAsset.h"
#include "Render/Core/RenderGraphDefinition.h"
using namespace MXRender::Asset;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;
using namespace MXRender::RHI::Vulkan;
using namespace MXRender;


Vector<UInt32> ReadShader(CONST String& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	CHECK_WITH_LOG(!file.is_open(), " App Error: fail to open the shader file! ")

	size_t fileSize = (size_t)file.tellg();
	Vector<UInt32> buffer(fileSize / sizeof(UInt32));

	file.seekg(0);
	file.read((char*)buffer.data(), fileSize);

	file.close();

	return std::move(buffer);
}

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RenderTest,public MXRender::RenderInterface)
#pragma region METHOD
public:
	RenderTest(PlatformWindow* in_window);
	RenderTest() MYDEFAULT;
	VIRTUAL ~RenderTest() MYDEFAULT;

	VIRTUAL void OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport) OVERRIDE FINAL;
	VIRTUAL void OnShutdown_Logic() OVERRIDE FINAL;
	VIRTUAL void OnUpdate(float dt) OVERRIDE FINAL;
	VIRTUAL void OnRender() OVERRIDE FINAL;
	

	PlatformWindow* GetPlatformWindow();
	RHI::Viewport* m_viewport = nullptr;
protected:

private:

#pragma endregion

#pragma region MEMBER
public:

protected:
	PlatformWindow* m_window;
private:

#pragma endregion

MYRENDERER_END_CLASS

void RenderTest::OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport)
{
	m_window = in_window;
	m_viewport = in_viewport;
	std::cout << "Hello CubeMap" << std::endl;

	RHI::CommandList* cmd_list = RHIGetImmediateCommandList();

	// Step 1: Register external resources (BackBuffer + DepthStencil)
	RHI::Texture* backbuffer_rtv = in_viewport->GetCurrentBackBufferRTV();
	RHI::Texture* backbuffer_dsv = in_viewport->GetCurrentBackBufferDSV();

	RHI::TextureDesc rt_desc = backbuffer_rtv->GetTextureDesc();
	auto* rt_resource = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
		"BackBuffer", rt_desc, backbuffer_rtv);

	RenderGraphResource<RHI::TextureDesc, RHI::Texture>* ds_resource = nullptr;
	if (backbuffer_dsv)
	{
		RHI::TextureDesc ds_desc = backbuffer_dsv->GetTextureDesc();
		ds_resource = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
			"DepthStencil", ds_desc, backbuffer_dsv);
	}

	// Step 2: Load cubemap texture asset and register as retained resource
	TextureAsset* cubemap_asset = new TextureAsset("Texture/Skybox/bolonga_lod.dds");
	RHI::Texture* tex_ptr = cubemap_asset->GetTexture();
	RHI::TextureDesc tex_desc;
	if (tex_ptr) tex_desc = tex_ptr->GetTextureDesc();
	auto* cubemap_resource = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
		"SkyboxCubemap", tex_desc, tex_ptr);

	// Step 3: Add render pass with proper RDG resource declarations
	struct TestData :public RenderGraphPassDataBase
	{
		RenderPipelineState* pipeline_state = nullptr;
		ShaderResourceBinding* srb = nullptr;
		TextureAsset* bind_texture = nullptr;
		VIRTUAL ~TestData() { Release(); }
		void Release()
		{
			if (srb) { delete srb; srb = nullptr; }
			if (bind_texture) { delete bind_texture; bind_texture = nullptr; }
		}
	};

	auto* rdg_pass = graph.AddRenderPass<TestData>("MainPass",&graph, cmd_list,
	[&](TestData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd_list)
	{
		// Declare resource dependencies
		builder.Read(cubemap_resource);
		builder.Write(rt_resource);
		if (ds_resource) builder.Write(ds_resource);

		// Create pipeline state
		ShaderDesc shader_desc;
		ShaderDataPayload shader_data;
		RenderGraphiPipelineStateDesc pipeline_state_desc;

		shader_desc.shader_type = ENUM_SHADER_STAGE::Shader_Vertex;
		shader_desc.shader_name = "SkyboxVS";
		shader_data.data = ReadShader("Shader/skybox_test.vert.spv");
		Shader* vs_shader = RHICreateShader(shader_desc,shader_data);

		shader_desc.shader_type = ENUM_SHADER_STAGE::Shader_Pixel;
		shader_desc.shader_name = "SkyboxPS";
		shader_data.data = ReadShader("Shader/skybox_test.frag.spv");
		Shader* ps_shader = RHICreateShader(shader_desc, shader_data);

		pipeline_state_desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs_shader;
		pipeline_state_desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps_shader;
		pipeline_state_desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		Vector<Texture*> rtvs = { this->m_viewport->GetCurrentBackBufferRTV() };
		pipeline_state_desc.render_targets = rtvs;
		pipeline_state_desc.depth_stencil_view = backbuffer_dsv;
		pipeline_state_desc.raster_state.sample_count = 1;
		pipeline_state_desc.blend_state.render_targets.resize(rtvs.size());

		data.pipeline_state = g_render_rhi->CreateRenderPipelineState(pipeline_state_desc);
		data.pipeline_state->CreateShaderResourceBinding(data.srb,true);

		data.bind_texture = cubemap_asset;
		delete vs_shader;
		delete ps_shader;
	},
	[=](CONST TestData& data, CommandList* in_cmd_list)
	{
		if (data.bind_texture->GetTexture())
		{
			Vector<ClearValue> clear_values;
			Vector<Texture*> rtvs = { this->m_viewport->GetCurrentBackBufferRTV() };
			Texture* dsv = this->m_viewport->GetCurrentBackBufferDSV();
			for (auto rtv : rtvs)
				clear_values.push_back(rtv->GetTextureDesc().clear_value);
			if (dsv)
				clear_values.push_back(dsv->GetTextureDesc().clear_value);
			in_cmd_list->SetRenderTarget(rtvs, dsv, clear_values, dsv != nullptr);
			in_cmd_list->SetGraphicsPipeline(data.pipeline_state);
			data.srb->SetResource("cubemap_sampler", data.bind_texture->GetTexture());
			in_cmd_list->SetShaderResourceBinding(data.srb);
			DrawAttribute draw_attr;
			draw_attr.vertexCount = 6;
			draw_attr.instanceCount = 1;
			in_cmd_list->Draw(draw_attr);
		}
	});

	rdg_pass->SetIsCullable(false);
	graph.Compile();
}

void RenderTest::OnShutdown_Logic()
{
	Render::RenderGraphDefinition def;
	def.graph_name = "CubeMap";
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
		for (auto* r : pass->GetReadResources())  pd.read_resources.push_back(r->GetName());
		for (auto* w : pass->GetWriteResources()) pd.write_resources.push_back(w->GetName());
		for (auto* c : pass->GetCreateResources()) pd.create_resources.push_back(c->GetName());
		def.passes.push_back(pd);
	}

	Render::RenderGraphSerializer::SaveGraph(def, "cubemap.rgraph.json");
	std::cout << "[Sample] Graph saved to cubemap.rgraph.json" << std::endl;

		graph.Release();
}

void RenderTest::OnUpdate(float dt) {}

void RenderTest::OnRender() { graph.Execute(); }



RenderTest::RenderTest(PlatformWindow* in_window):m_window(in_window) {}

MXRender::PlatformWindow* RenderTest::GetPlatformWindow() { return m_window; }

int main() {
	Window window;
	RenderTest render(window.GetPlatformWindow());
	window.InitWindow();
	window.Run(&render);
	return 0;
}
