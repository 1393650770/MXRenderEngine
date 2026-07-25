// Sample 11-Mesh: first real user of the vertex-input pipeline.
// Integration test for: MeshLoader/MeshAsset (obj -> VB/IB), SceneView +
// OrbitCameraController (unified Vulkan depth/Y conventions), SampleApp
// boilerplate base, ShaderLibrary/BufferUtils, and DrawIndexedIndirect
// (press I to toggle indirect vs direct draw - the image must not change).
#include "Application/SampleApp.h"
#include "Application/CameraController.h"   // pulls SceneView.h (glm 0..1 depth) first
#include "Application/Window.h"
#include "Asset/MeshAsset.h"
#include "Tool/MeshLoader.h"
#include "Tool/ShaderLibrary.h"
#include "Tool/BufferUtils.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/RenderBuffer.h"
#include <iostream>
using namespace MXRender;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;

struct MeshPassData : public RenderGraphPassDataBase
{
	RenderPipelineState* pso = nullptr;
	ShaderResourceBinding* srb = nullptr;
	VIRTUAL ~MeshPassData() MYDEFAULT;
	VIRTUAL void Release() OVERRIDE { delete srb; srb = nullptr; }
};

// matches the `Params` readonly buffer in meshsample_lit.vert/.frag
struct ShaderParams
{
	glm::mat4 mvp;
	glm::mat4 model;
	glm::vec4 light_dir;
};

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(MeshSampleApp, public Application::SampleApp)
#pragma region METHOD
public:
	MeshSampleApp() MYDEFAULT;
	VIRTUAL ~MeshSampleApp() MYDEFAULT;

	VIRTUAL void OnInitScene() OVERRIDE FINAL;
	VIRTUAL void OnShutdownScene() OVERRIDE FINAL;
	VIRTUAL void OnUpdate(float dt) OVERRIDE FINAL;
protected:

private:

#pragma endregion

#pragma region MEMBER
public:

protected:
	Asset::MeshAsset mesh{ "Mesh/cube.obj" };
	Render::SceneView scene_view;
	Application::OrbitCameraController camera;
	RHI::Buffer* param_buf = nullptr;
	RHI::Buffer* indirect_buf = nullptr;
	ShaderParams cached_params{};
	Bool use_indirect = false;
	Bool indirect_key_prev = false;
private:

#pragma endregion
MYRENDERER_END_CLASS

void MeshSampleApp::OnInitScene()
{
	std::cout << "Hello Mesh" << std::endl;

	// mesh must be resident before PSO/SRB creation (RDG compiles at init)
	mesh.WaitUntilLoaded();
	RHI::Buffer* vb = mesh.GetVertexBuffer();
	RHI::Buffer* ib = mesh.GetIndexBuffer();
	CHECK_WITH_LOG(vb == nullptr || ib == nullptr, "MeshSample Error: failed to load Mesh/cube.obj")
	std::cout << "[MeshSample] vertices=" << mesh.GetVertexCount()
		<< " indices=" << mesh.GetIndexCount() << std::endl;

	param_buf = Tool::BufferUtils::CreateDynamicParamBuffer(sizeof(ShaderParams));

	// indirect draw args: Indirect|Dynamic exercises the combined-usage
	// translation; CPU fills the args once (a compute pass could as well)
	DrawIndexedIndirectArgs args;
	args.index_count = mesh.GetIndexCount();
	args.instance_count = 1;
	RHI::BufferDesc ind_desc;
	ind_desc.size = (UInt32)sizeof(DrawIndexedIndirectArgs);
	ind_desc.stride = (UInt32)sizeof(DrawIndexedIndirectArgs);
	ind_desc.type = ENUM_BUFFER_TYPE::Indirect | ENUM_BUFFER_TYPE::Dynamic;
	indirect_buf = g_render_rhi->CreateBuffer(ind_desc);
	Tool::BufferUtils::Upload(indirect_buf, &args, sizeof(args));

	camera.Attach(GetPlatformWindow());
	camera.distance = 5.0f;
	scene_view.SetPerspective(glm::radians(45.0f), 0.1f, 100.0f);

	// scripted-verification override: MESHSAMPLE_INDIRECT=1 starts on the
	// indirect path (same as pressing I) so screenshots can diff both paths
	if (std::getenv("MESHSAMPLE_INDIRECT") != nullptr)
	{
		use_indirect = true;
		std::cout << "[MeshSample] draw path: DrawIndexedIndirect (env)" << std::endl;
	}

	auto* pass = graph.AddRenderPass<MeshPassData>("MeshPass", &graph, RHIGetImmediateCommandList(),
	[&](MeshPassData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd)
	{
		builder.Write(GetBackBufferResource());
		if (GetDepthStencilResource()) builder.Write(GetDepthStencilResource());

		Shader* vs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Vertex, "Shader/meshsample_lit.vert.spv");
		Shader* ps = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Pixel, "Shader/meshsample_lit.frag.spv");

		RenderGraphiPipelineStateDesc pd{};
		pd.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
		pd.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps;
		pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		pd.vertex_input_layout = Tool::MeshDataPayload::GetVertexInputLayout();
		pd.render_targets = { GetBackBuffer() };
		pd.depth_stencil_view = GetDepthStencil();
		pd.raster_state.sample_count = 1;
		pd.raster_state.cull_mode = ENUM_RASTER_CULLMODE::None;
		pd.depth_stencil_state.depth_test_enable = true;
		pd.depth_stencil_state.depth_write_enable = true;
		pd.depth_stencil_state.depth_func = ENUM_STENCIL_FUNCTION::ENUM_LESS;
		pd.blend_state.render_targets.resize(1);
		data.pso = g_render_rhi->CreateRenderPipelineState(pd);
		data.pso->CreateShaderResourceBinding(data.srb, false);
		data.srb->SetResource("params", param_buf);
		data.srb->FlushDescriptorWrites();
		delete vs;
		delete ps;
	},
	[=](CONST MeshPassData& data, CommandList* in_cmd)
	{
		BindBackBufferTarget(in_cmd);
		Tool::BufferUtils::Upload(param_buf, &cached_params, sizeof(ShaderParams));
		in_cmd->SetGraphicsPipeline(data.pso);
		in_cmd->SetShaderResourceBinding(data.srb);
		in_cmd->SetVertexBuffer(mesh.GetVertexBuffer(), 0, Tool::MeshDataPayload::GetVertexStride(), 0);
		in_cmd->SetIndexBuffer(mesh.GetIndexBuffer(), 0, true);
		if (use_indirect)
			in_cmd->DrawIndexedIndirect(indirect_buf, 0, 1);
		else
			in_cmd->DrawIndexed(mesh.GetIndexCount(), 1, 0, 0, 0);
	});
	pass->SetIsCullable(false);
	pass->SetShaderPath("Shader/meshsample_lit");
}

void MeshSampleApp::OnShutdownScene()
{
	// release GPU resources BEFORE RHIShutdown (member destruction is too late)
	mesh.ReleaseBuffers();
	delete param_buf;
	param_buf = nullptr;
	delete indirect_buf;
	indirect_buf = nullptr;
}

void MeshSampleApp::OnUpdate(float dt)
{
	camera.Update(dt, scene_view);

	#if 0 // TODO: keyboard via PlatformWindow (GLFW not portable)
	void* w = GetPlatformWindow()->GetNativeHandle();
	Bool key_now = glfwGetKey(w, GLFW_KEY_I) == GLFW_PRESS;
#endif
	Bool key_now = false; // stub until PlatformWindow keyboard
	if (key_now && !indirect_key_prev)
	{
		use_indirect = !use_indirect;
		std::cout << "[MeshSample] draw path: " << (use_indirect ? "DrawIndexedIndirect" : "DrawIndexed") << std::endl;
	}
	indirect_key_prev = key_now;

	cached_params.mvp = scene_view.GetViewProjectionMatrix(); // model = identity
	cached_params.model = glm::mat4(1.0f);
	cached_params.light_dir = glm::vec4(glm::normalize(glm::vec3(0.4f, 0.8f, 0.45f)), 0.0f);
}

int main()
{
	MeshSampleApp app;
	return MXRender::Application::SampleApp::RunSample(app, "Sample 11 - Mesh");
}
