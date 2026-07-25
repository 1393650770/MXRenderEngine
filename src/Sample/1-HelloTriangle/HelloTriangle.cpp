// Sample 1-HelloTriangle, migrated onto Application::SampleApp - serves as
// the regression reference for the base class (the image must be identical
// to the pre-migration version).
#include "Application/SampleApp.h"
#include "Platform/Android/AndroidEntry.h"
#include "Tool/ShaderLibrary.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/ResourceManager.h"
#include <iostream>
using namespace MXRender;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;

struct TriPassData : public RenderGraphPassDataBase
{
	PSOHandle pipeline_state;
	ShaderResourceBinding* srb = nullptr;
	VIRTUAL ~TriPassData() { Release(); }
	void Release()
	{
		if (srb) { delete srb; srb = nullptr; }
	}
};

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RenderTest, public Application::SampleApp)
#pragma region METHOD
public:
	RenderTest() MYDEFAULT;
	VIRTUAL ~RenderTest() MYDEFAULT;

	VIRTUAL void OnInitScene() OVERRIDE FINAL;
	VIRTUAL void OnShutdownScene() OVERRIDE FINAL;
protected:

private:

#pragma endregion
MYRENDERER_END_CLASS

void RenderTest::OnInitScene()
{
	std::cout << "Hello Triangle" << std::endl;

	auto* rdg_pass = graph.AddRenderPass<TriPassData>("MainPass", &graph, RHIGetImmediateCommandList(),
	[&](TriPassData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd_list)
	{
		builder.Write(GetBackBufferResource());
		if (GetDepthStencilResource()) builder.Write(GetDepthStencilResource());

		Shader* vs_shader = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Vertex, "Shader/triangle_test.vert.spv");
		Shader* ps_shader = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Pixel, "Shader/triangle_test.frag.spv");

		RenderGraphiPipelineStateDesc pipeline_state_desc;
		pipeline_state_desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs_shader;
		pipeline_state_desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps_shader;
		pipeline_state_desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		pipeline_state_desc.render_targets = { GetBackBuffer() };
		pipeline_state_desc.depth_stencil_view = GetDepthStencil();
		pipeline_state_desc.raster_state.sample_count = 1;
		pipeline_state_desc.blend_state.render_targets.resize(1);

		data.pipeline_state = Create<PSOHandle>(pipeline_state_desc, "MainPass");
		auto* _pso_ = Resolve(data.pipeline_state); if (_pso_) _pso_->CreateShaderResourceBinding(data.srb);
		delete vs_shader;
		delete ps_shader;
	},
	[=](CONST TriPassData& data, CommandList* in_cmd_list)
	{
		BindBackBufferTarget(in_cmd_list);
		in_cmd_list->SetGraphicsPipeline(Resolve(data.pipeline_state));
		in_cmd_list->SetShaderResourceBinding(data.srb);
		DrawAttribute draw_attr;
		draw_attr.vertexCount = 3;
		draw_attr.instanceCount = 1;
		in_cmd_list->Draw(draw_attr);
	});

	rdg_pass->SetIsCullable(false);
	rdg_pass->SetShaderPath("Shader/triangle_test");
	rdg_pass->SetVertexCount(3);
}

void RenderTest::OnShutdownScene()
{
	// Serialize RDG to JSON for Editor loading
	SaveGraphDefinition("HelloTriangle", "hello_triangle.rgraph.json");
}

int main()
{
	RenderTest app;
	return MXRender::Application::SampleApp::RunSample(app);
}
