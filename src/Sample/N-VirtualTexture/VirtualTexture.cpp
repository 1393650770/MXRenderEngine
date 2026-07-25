#include<iostream>
#include <fstream>
#include <map>
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
#include "RHI/ResourceManager.h"
#include "Asset/TextureAsset.h"
#include "Render/Core/VirtualTexture/VirtualTextureDataQualTree.h"
using namespace MXRender::Asset;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Render::Core;
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

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RenderTest, public MXRender::RenderInterface)
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
	RenderGraph graph;
	PlatformWindow* m_window;
private:

#pragma endregion

MYRENDERER_END_CLASS

void RenderTest::OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport)
{
	std::cout << "Hello Texture" << std::endl;

	struct TestData :public RenderGraphPassDataBase
	{

		PSOHandle pipeline_state;
		ShaderResourceBinding* srb = nullptr;
		TextureAsset* bind_texture = nullptr;
		VIRTUAL ~TestData()
		{
			Release();
		}
		void Release()
		{
			if (srb)
			{
				delete srb;
				srb = nullptr;
			}
			if (bind_texture)
			{
				delete bind_texture;
				bind_texture = nullptr;
			}
		}
	};
	RenderPassDesc renderpass_desc;
	CommandList* cmd_list = RHIGetImmediateCommandList();
	auto* rdg_pass = graph.AddRenderPass<TestData>("TestPass", &graph, cmd_list,
		[&](TestData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd_list)
	{
		Shader* vs_shader;
		Shader* ps_shader;
		ShaderDesc shader_desc;
		ShaderDataPayload shader_data;
		RenderGraphiPipelineStateDesc pipeline_state_desc;
		TextureDesc texture_desc;
		TextureDataPayload texture_data;
		shader_desc.shader_type = ENUM_SHADER_STAGE::Shader_Vertex;
		shader_desc.shader_name = "TestVS";
		shader_data.data = ReadShader("Shader/skybox_test.vert.spv");
		vs_shader = RHICreateShader(shader_desc, shader_data);

		shader_desc.shader_type = ENUM_SHADER_STAGE::Shader_Pixel;
		shader_desc.shader_name = "TestPS";
		shader_data.data = ReadShader("Shader/skybox_test.frag.spv");
		ps_shader = RHICreateShader(shader_desc, shader_data);

		pipeline_state_desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs_shader;
		pipeline_state_desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps_shader;
		pipeline_state_desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		Vector<Texture*> rtvs;
		Texture* dsv;
		rtvs = { this->m_viewport->GetCurrentBackBufferRTV() };
		dsv = this->m_viewport->GetCurrentBackBufferDSV();
		pipeline_state_desc.render_targets = rtvs;
		pipeline_state_desc.depth_stencil_view = dsv;
		pipeline_state_desc.raster_state.sample_count = 1;
		pipeline_state_desc.blend_state.render_targets.resize(rtvs.size());

		data.pipeline_state = Create<PSOHandle>(pipeline_state_desc, "MainPass");
		auto* _pso_ = Resolve(data.pipeline_state); if (_pso_) _pso_->CreateShaderResourceBinding(data.srb, true);

		data.bind_texture = new TextureAsset("Texture/Skybox/bolonga_lod.dds");
		delete vs_shader;
		delete ps_shader;
	},
		[=](CONST TestData& data, CommandList* in_cmd_list)
	{
		if (data.bind_texture->GetTexture())
		{
			Vector<ClearValue> clear_values;
			Vector<Texture*> rtvs;
			Texture* dsv;
			rtvs = { this->m_viewport->GetCurrentBackBufferRTV() };
			dsv = this->m_viewport->GetCurrentBackBufferDSV();
			for (auto rtv : rtvs)
			{
				clear_values.push_back(rtv->GetTextureDesc().clear_value);
			}
			if (dsv)
				clear_values.push_back(dsv->GetTextureDesc().clear_value);
			in_cmd_list->SetRenderTarget(rtvs, dsv, clear_values, dsv != nullptr);
			in_cmd_list->SetGraphicsPipeline(Resolve(data.pipeline_state));
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
	graph.Release();
}

void RenderTest::OnUpdate(float dt)
{

}

void RenderTest::OnRender()
{
	graph.Execute();
	//RHISubmitCommandList(RHIGetImmediateCommandList());
}



RenderTest::RenderTest(PlatformWindow* in_window) :m_window(in_window)
{

}

MXRender::PlatformWindow* RenderTest::GetPlatformWindow()
{
	return m_window;
}

int main() {
	Window window;
	RenderTest render(window.GetPlatformWindow());
	window.InitWindow();
	window.Run(&render);
	return 0;
}
