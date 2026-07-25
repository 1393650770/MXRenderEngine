#if PLATFORM_GLES3
// MiniGame Texture Sample — Phase 1 deliverable
// Verifies: Texture creation + upload, SRB texture binding, RenderGraph pass.
// Fullscreen quad via gl_VertexID (no vertex buffer) sampling a checkerboard texture.

#include "Application/SampleApp.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/GLES3/GLES3_Shader.h"
#include "RHI/GLES3/GLES3_PipelineState.h"
#include "RHI/GLES3/GLES3_Texture.h"
#include "RHI/GLES3/GLES3_ShaderResourceBinding.h"
#include <cstring>
#include <iostream>

using namespace MXRender;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;

// ---- GLSL ES 300 es shaders (6-vertex fullscreen quad via gl_VertexID) ----

static const char* g_tex_vs = R"(#version 300 es
precision highp float;
const vec2 pos[6] = vec2[6](
    vec2(-1.0,  1.0), vec2(-1.0, -1.0), vec2( 1.0,  1.0),
    vec2( 1.0,  1.0), vec2(-1.0, -1.0), vec2( 1.0, -1.0)
);
const vec2 uv[6] = vec2[6](
    vec2(0.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 1.0),
    vec2(1.0, 1.0), vec2(0.0, 0.0), vec2(1.0, 0.0)
);
out vec2 v_uv;
void main() {
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
    v_uv = uv[gl_VertexID];
}
)";

static const char* g_tex_fs = R"(#version 300 es
precision highp float;
in vec2 v_uv;
out vec4 fragColor;
uniform sampler2D u_texture;
void main() {
    fragColor = texture(u_texture, v_uv);
}
)";

// ---- Pass data ----
struct TexPassData : public RenderGraphPassDataBase
{
	RenderPipelineState* pso = nullptr;
	ShaderResourceBinding* srb = nullptr;
	VIRTUAL ~TexPassData() { Release(); }
	void Release()
	{
		if (srb) { delete srb; srb = nullptr; }
	}
};

// ---- Sample class ----
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(MiniGameTexture, public Application::SampleApp)
#pragma region METHOD
public:
	MiniGameTexture() MYDEFAULT;
	VIRTUAL ~MiniGameTexture() MYDEFAULT;
	VIRTUAL void OnInitScene() OVERRIDE FINAL;
	VIRTUAL void OnShutdownScene() OVERRIDE FINAL;
protected:
private:
#pragma endregion

	GLES3::GLES3_Texture* m_texture = nullptr;
MYRENDERER_END_CLASS

static void BuildCheckerboard(UInt32* pixels, UInt32 w, UInt32 h, UInt32 tile)
{
	for (UInt32 y = 0; y < h; ++y)
		for (UInt32 x = 0; x < w; ++x)
			pixels[y * w + x] = ((x / tile) + (y / tile)) % 2 == 0
				? 0xFFFFFFFF : 0xFF0080FF;
}

void MiniGameTexture::OnInitScene()
{
	std::cout << "[MiniGame] Texture Sample: OnInitScene" << std::endl;

	// ---- Create procedural 256x256 RGBA8 checkerboard texture ----
	const UInt32 tw = 256, th = 256;
	Vector<UInt32> pixels(tw * th);
	BuildCheckerboard(pixels.data(), tw, th, 32);

	TextureDesc tex_desc;
	tex_desc.width = tw;
	tex_desc.height = th;
	tex_desc.format = ENUM_TEXTURE_FORMAT::RGBA8;
	tex_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	tex_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
	tex_desc.mip_level = 1;

	m_texture = static_cast<GLES3::GLES3_Texture*>(g_render_rhi->CreateTexture(tex_desc));
	// Upload pixel data (bypass RHI — direct GL call is simplest for Phase 1)
	GLuint gl_tex = m_texture->GetGLTexture();
	if (gl_tex)
	{
		glBindTexture(GL_TEXTURE_2D, gl_tex);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)tw, (GLsizei)th,
			GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// ---- Create shaders ----
	ShaderDesc vs_desc;
	vs_desc.shader_type = ENUM_SHADER_STAGE::Shader_Vertex;
	vs_desc.debug_name = "tex_vs";
	vs_desc.entry_name = "main";
	ShaderDataPayload vs_payload;
	vs_payload.wgsl_source = g_tex_vs;
	Shader* vs = RHICreateShader(vs_desc, vs_payload);
	static_cast<GLES3::GLES3_Shader*>(vs)->SetGLSLSource(g_tex_vs);

	ShaderDesc fs_desc;
	fs_desc.shader_type = ENUM_SHADER_STAGE::Shader_Pixel;
	fs_desc.debug_name = "tex_fs";
	fs_desc.entry_name = "main";
	ShaderDataPayload fs_payload;
	fs_payload.wgsl_source = g_tex_fs;
	Shader* fs = RHICreateShader(fs_desc, fs_payload);
	static_cast<GLES3::GLES3_Shader*>(fs)->SetGLSLSource(g_tex_fs);

	// ---- Register RenderGraph pass ----
	auto* rdg_pass = graph.AddRenderPass<TexPassData>("TexturePass", &graph, RHIGetImmediateCommandList(),
		[&](TexPassData& data, RenderGraphPassBuilder& builder, CommandList*)
		{
			builder.Write(GetBackBufferResource());

			RenderGraphiPipelineStateDesc pso_desc;
			pso_desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
			pso_desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = fs;
			pso_desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
			pso_desc.render_targets = { GetBackBuffer() };
			pso_desc.raster_state.sample_count = 1;
			pso_desc.depth_stencil_state.depth_test_enable = false;

			data.pso = g_render_rhi->CreateRenderPipelineState(pso_desc);
			if (data.pso) data.pso->CreateShaderResourceBinding(data.srb);

			// Bind checkerboard texture to the SRB
			auto* gl_srb = static_cast<GLES3::GLES3_ShaderResourceBinding*>(data.srb);
			if (gl_srb && m_texture)
				gl_srb->SetResource("u_texture", m_texture);
		},
		[this](CONST TexPassData& data, CommandList* in_cmd_list)
		{
			BindBackBufferTarget(in_cmd_list);
			in_cmd_list->SetGraphicsPipeline(data.pso);
			in_cmd_list->SetShaderResourceBinding(data.srb);

			DrawAttribute draw_attr;
			draw_attr.vertexCount = 6;  // 2 triangles = 6 vertices
			draw_attr.instanceCount = 1;
			in_cmd_list->Draw(draw_attr);
		});

	rdg_pass->SetIsCullable(false);
	rdg_pass->SetShaderPath("minigame/texture");
	rdg_pass->SetVertexCount(6);
}

void MiniGameTexture::OnShutdownScene()
{
	if (m_texture) { delete m_texture; m_texture = nullptr; }
}

int main()
{
	MiniGameTexture app;
	return Application::SampleApp::RunSample(app, "MiniGame Texture");
}

#endif // PLATFORM_GLES3
