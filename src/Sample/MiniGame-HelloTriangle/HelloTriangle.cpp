#if PLATFORM_GLES3
// MiniGame HelloTriangle Sample — Phase 0 deliverable
// Verifies the GLES3 RHI backend pipeline end-to-end.
// Hardcoded triangle via gl_VertexIndex (no vertex buffers).

#include "Application/SampleApp.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/GLES3/GLES3_Shader.h"
#include "RHI/GLES3/GLES3_PipelineState.h"

using namespace MXRender;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;

// ---- GLSL ES 300 es shaders (embedded as C++ strings) ----

static const char* g_triangle_vs = R"(#version 300 es
precision highp float;
const vec3 pos[3] = vec3[3](
    vec3( 0.0,  0.5, 0.0),
    vec3(-0.5, -0.5, 0.0),
    vec3( 0.5, -0.5, 0.0)
);
const vec3 col[3] = vec3[3](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);
out vec3 v_color;
void main() {
    gl_Position = vec4(pos[gl_VertexID], 1.0);
    v_color = col[gl_VertexID];
}
)";

static const char* g_triangle_fs = R"(#version 300 es
precision highp float;
in vec3 v_color;
out vec4 fragColor;
void main() {
    fragColor = vec4(v_color, 1.0);
}
)";

// ---- Pass data (RenderGraphPassDataBase pattern) ----

struct TriPassData : public RenderGraphPassDataBase
{
	RenderPipelineState* pso = nullptr;
	ShaderResourceBinding* srb = nullptr;
	VIRTUAL ~TriPassData() { Release(); }
	void Release()
	{
		if (srb) { delete srb; srb = nullptr; }
		// PSO owned by GLES3_RenderRHI::m_pso_storage, do NOT delete pso
	}
};

// ---- Sample class ----

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(MiniGameHelloTriangle, public Application::SampleApp)
#pragma region METHOD
public:
	MiniGameHelloTriangle() MYDEFAULT;
	VIRTUAL ~MiniGameHelloTriangle() MYDEFAULT;
	VIRTUAL void OnInitScene() OVERRIDE FINAL;
	VIRTUAL void OnShutdownScene() OVERRIDE FINAL;
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

void MiniGameHelloTriangle::OnInitScene()
{
	std::cout << "[MiniGame] HelloTriangle: OnInitScene" << std::endl;

	// ---- Create shaders with GLSL ES source ----
	ShaderDesc vs_desc;
	vs_desc.shader_type = ENUM_SHADER_STAGE::Shader_Vertex;
	vs_desc.debug_name = "triangle_vs";
	vs_desc.entry_name = "main";

	ShaderDataPayload vs_payload;
	vs_payload.wgsl_source = g_triangle_vs;  // reuse wgsl_source field for GLSL

	Shader* vs_shader = RHICreateShader(vs_desc, vs_payload);
	static_cast<GLES3::GLES3_Shader*>(vs_shader)->SetGLSLSource(g_triangle_vs);

	ShaderDesc fs_desc;
	fs_desc.shader_type = ENUM_SHADER_STAGE::Shader_Pixel;
	fs_desc.debug_name = "triangle_fs";
	fs_desc.entry_name = "main";

	ShaderDataPayload fs_payload;
	fs_payload.wgsl_source = g_triangle_fs;

	Shader* fs_shader = RHICreateShader(fs_desc, fs_payload);
	static_cast<GLES3::GLES3_Shader*>(fs_shader)->SetGLSLSource(g_triangle_fs);

	// ---- Register RenderGraph pass ----
	auto* rdg_pass = graph.AddRenderPass<TriPassData>("MainPass", &graph, RHIGetImmediateCommandList(),
		[&](TriPassData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd_list)
		{
			builder.Write(GetBackBufferResource());
			// No depth/stencil needed for this triangle demo

			RenderGraphiPipelineStateDesc pso_desc;
			pso_desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs_shader;
			pso_desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = fs_shader;
			pso_desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
			pso_desc.render_targets = { GetBackBuffer() };
			pso_desc.raster_state.sample_count = 1;
			pso_desc.blend_state.render_targets.resize(1);
			pso_desc.depth_stencil_state.depth_test_enable = false;

			data.pso = g_render_rhi->CreateRenderPipelineState(pso_desc);
			if (data.pso) data.pso->CreateShaderResourceBinding(data.srb);
		},
		[this](CONST TriPassData& data, CommandList* in_cmd_list)
		{
			BindBackBufferTarget(in_cmd_list);
			in_cmd_list->SetGraphicsPipeline(data.pso);
			in_cmd_list->SetShaderResourceBinding(data.srb);

			DrawAttribute draw_attr;
			draw_attr.vertexCount = 3;
			draw_attr.instanceCount = 1;
			in_cmd_list->Draw(draw_attr);
		});

	rdg_pass->SetIsCullable(false);
	rdg_pass->SetShaderPath("minigame/triangle");
	rdg_pass->SetVertexCount(3);

	// Shaders were consumed by PSO creation; they're kept alive by the PSO (GL program
	// keeps attached shaders alive after linking). We can drop our refs.
	// delete vs_shader; delete fs_shader;  // held by PSO via GL program -> skip
}

void MiniGameHelloTriangle::OnShutdownScene()
{
	// Graph compilation + PSO/SRB cleanup handled by RenderGraphPassDataBase::Release()
}

// ---- Entry point ----
int main()
{
	MiniGameHelloTriangle app;
	return Application::SampleApp::RunSample(app, "MiniGame HelloTriangle");
}

#endif // PLATFORM_GLES3
