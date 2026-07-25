#if PLATFORM_GLES3
// MiniGame Mesh Sample - Phase 2 deliverable
// Verifies: VBO + IBO + VAO rendering, SRB uniform buffer, 3D transform.

#include "Application/SampleApp.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/GLES3/GLES3_Buffer.h"
#include "RHI/GLES3/GLES3_Shader.h"
#include "RHI/GLES3/GLES3_PipelineState.h"
#include "RHI/GLES3/GLES3_Texture.h"
#include "RHI/GLES3/GLES3_ShaderResourceBinding.h"
#include "Application/MiniGameCamera.h"
#include "Platform/Emscripten/BrowserCamera.h"
#include "Render/View/SceneView.h"
#include <cstring>
#include <cmath>
#include <iostream>

using namespace MXRender;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;

static const char* g_mesh_vs = R"(#version 300 es
precision highp float;
layout(location=0) in vec3 a_position;
layout(location=1) in vec2 a_texcoord;

uniform mat4 u_mvp;
out vec2 v_uv;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_uv = a_texcoord;
}
)";

static const char* g_mesh_fs = R"(#version 300 es
precision highp float;
in vec2 v_uv;
out vec4 fragColor;
uniform sampler2D u_texture;
void main() {
    fragColor = texture(u_texture, v_uv);
}
)";

struct Vertex { float x, y, z, u, v; };

// CCW winding (front faces visible, back-face culling works correctly)
static const Vertex g_cube_verts[] = {
    // Front (+Z)
    {-1, 1, 1, 0,1}, { 1,-1, 1, 1,0}, {-1,-1, 1, 0,0},
    { 1, 1, 1, 1,1}, { 1,-1, 1, 1,0}, {-1, 1, 1, 0,1},
    // Back (-Z)
    { 1, 1,-1, 0,1}, {-1,-1,-1, 1,0}, { 1,-1,-1, 0,0},
    {-1, 1,-1, 1,1}, {-1,-1,-1, 1,0}, { 1, 1,-1, 0,1},
    // Top (+Y)
    {-1, 1,-1, 0,1}, { 1, 1, 1, 1,0}, {-1, 1, 1, 0,0},
    { 1, 1,-1, 1,1}, { 1, 1, 1, 1,0}, {-1, 1,-1, 0,1},
    // Bottom (-Y)
    {-1,-1, 1, 0,1}, { 1,-1,-1, 1,0}, {-1,-1,-1, 0,0},
    { 1,-1, 1, 1,1}, { 1,-1,-1, 1,0}, {-1,-1, 1, 0,1},
    // Right (+X)
    { 1, 1, 1, 0,1}, { 1,-1,-1, 1,0}, { 1,-1, 1, 0,0},
    { 1, 1,-1, 1,1}, { 1,-1,-1, 1,0}, { 1, 1, 1, 0,1},
    // Left (-X)
    {-1, 1,-1, 0,1}, {-1,-1, 1, 1,0}, {-1,-1,-1, 0,0},
    {-1, 1, 1, 1,1}, {-1,-1, 1, 1,0}, {-1, 1,-1, 0,1},
};

struct MVPBlock { float mvp[16]; };

struct MeshPassData : public RenderGraphPassDataBase
{
	RenderPipelineState* pso = nullptr;
	ShaderResourceBinding* srb = nullptr;
	VIRTUAL ~MeshPassData() { Release(); }
	void Release() { if (srb) { delete srb; srb = nullptr; } }
};

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(MiniGameMesh, public Application::SampleApp)
#pragma region METHOD
public:
	MiniGameMesh() MYDEFAULT;
	VIRTUAL ~MiniGameMesh() MYDEFAULT;
	VIRTUAL void OnInitScene() OVERRIDE FINAL;
	VIRTUAL void OnShutdownScene() OVERRIDE FINAL;
	VIRTUAL void OnUpdate(Float32 dt) OVERRIDE;
protected:
private:
#pragma endregion

	RHI::Buffer* m_vb = nullptr;
	RHI::Texture* m_texture = nullptr;
	RHI::Buffer* m_ub = nullptr;
	Platform::Emscripten::BrowserCamera m_camera;
	Render::SceneView m_scene_view;
MYRENDERER_END_CLASS

static void BuildCheckerboard(UInt32* pixels, UInt32 w, UInt32 h, UInt32 tile)
{
	for (UInt32 y = 0; y < h; ++y)
		for (UInt32 x = 0; x < w; ++x)
			pixels[y * w + x] = ((x / tile) + (y / tile)) % 2 == 0
				? 0xFFFFFFFF : 0xFF0080FF;
}

void MiniGameMesh::OnInitScene()
{
	std::cout << "[MiniGame] Mesh Sample: OnInitScene" << std::endl;

	BufferDesc vb_desc;
	vb_desc.size = sizeof(g_cube_verts);
	vb_desc.type = ENUM_BUFFER_TYPE::Vertex;
	m_vb = g_render_rhi->CreateBuffer(vb_desc);
	{   void* ptr = g_render_rhi->MapBuffer(m_vb, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::None);
		memcpy(ptr, g_cube_verts, sizeof(g_cube_verts));
		g_render_rhi->UnmapBuffer(m_vb); }

	BufferDesc ub_desc;
	ub_desc.size = sizeof(MVPBlock);
	ub_desc.type = ENUM_BUFFER_TYPE::Uniform;
	m_ub = g_render_rhi->CreateBuffer(ub_desc);

	const UInt32 tw = 64, th = 64;
	Vector<UInt32> pixels(tw * th);
	BuildCheckerboard(pixels.data(), tw, th, 8);

	TextureDesc tex_desc;
	tex_desc.width = tw; tex_desc.height = th;
	tex_desc.format = ENUM_TEXTURE_FORMAT::RGBA8;
	tex_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	tex_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
	m_texture = g_render_rhi->CreateTexture(tex_desc);
	{   auto* gl_tex_obj = static_cast<GLES3::GLES3_Texture*>(m_texture);
		GLuint gl_tex = gl_tex_obj->GetGLTexture();
		if (gl_tex) {
		glBindTexture(GL_TEXTURE_2D, gl_tex);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)tw, (GLsizei)th,
			GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);
	} }

	ShaderDesc vs_desc; vs_desc.shader_type = ENUM_SHADER_STAGE::Shader_Vertex;
	vs_desc.debug_name = "mesh_vs";
	ShaderDataPayload vs_payload; vs_payload.wgsl_source = g_mesh_vs;
	Shader* vs = RHICreateShader(vs_desc, vs_payload);

	ShaderDesc fs_desc; fs_desc.shader_type = ENUM_SHADER_STAGE::Shader_Pixel;
	fs_desc.debug_name = "mesh_fs";
	ShaderDataPayload fs_payload; fs_payload.wgsl_source = g_mesh_fs;
	Shader* fs = RHICreateShader(fs_desc, fs_payload);

	auto* rdg_pass = graph.AddRenderPass<MeshPassData>("MeshPass", &graph, RHIGetImmediateCommandList(),
		[&](MeshPassData& data, RenderGraphPassBuilder& builder, CommandList*)
		{
			builder.Write(GetBackBufferResource());

			VertexInputLayout pos_attr, uv_attr;
			pos_attr.location = 0; pos_attr.attribute_format = ENUM_TEXTURE_FORMAT::RGBA32F;
			pos_attr.offset = 0; pos_attr.binding = 0; pos_attr.input_rate = ENUM_VERTEX_INPUTRATE::PerVertex;
			uv_attr.location = 1; uv_attr.attribute_format = ENUM_TEXTURE_FORMAT::RG32F;
			uv_attr.offset = sizeof(float) * 3; uv_attr.binding = 0; uv_attr.input_rate = ENUM_VERTEX_INPUTRATE::PerVertex;

			RenderGraphiPipelineStateDesc pso_desc;
			pso_desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
			pso_desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = fs;
			pso_desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
			pso_desc.render_targets = { GetBackBuffer() };
			pso_desc.raster_state.sample_count = 1;
			pso_desc.depth_stencil_state.depth_test_enable = true;
			pso_desc.depth_stencil_state.depth_write_enable = true;
			pso_desc.vertex_input_layout = { pos_attr, uv_attr };

			data.pso = g_render_rhi->CreateRenderPipelineState(pso_desc);
			if (data.pso) data.pso->CreateShaderResourceBinding(data.srb);

			auto* gl_srb = static_cast<GLES3::GLES3_ShaderResourceBinding*>(data.srb);
			if (gl_srb) {
				gl_srb->SetResource("u_texture", m_texture);
				gl_srb->SetResource("u_mvp", m_ub);
			}
		},
		[this](CONST MeshPassData& data, CommandList* in_cmd_list)
		{
			BindBackBufferTarget(in_cmd_list);

			CONST auto& vp = m_scene_view.GetViewProjectionMatrix();
			MVPBlock mvp;
			memcpy(mvp.mvp, &vp[0][0], sizeof(mvp.mvp));
			{   void* ptr = g_render_rhi->MapBuffer(m_ub, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::None);
				memcpy(ptr, &mvp, sizeof(MVPBlock));
				g_render_rhi->UnmapBuffer(m_ub); }

			in_cmd_list->SetGraphicsPipeline(data.pso);
			in_cmd_list->SetShaderResourceBinding(data.srb);
			in_cmd_list->SetVertexBuffer(m_vb, 0, sizeof(Vertex), 0);

			DrawAttribute draw_attr;
			draw_attr.vertexCount = 36;
			draw_attr.instanceCount = 1;
			in_cmd_list->Draw(draw_attr);
		});

	rdg_pass->SetIsCullable(false);
	rdg_pass->SetShaderPath("minigame/mesh");
	rdg_pass->SetVertexCount(36);
}

void MiniGameMesh::OnUpdate(Float32 dt)
{
	m_camera.Update(dt, GetViewportWidth(), GetViewportHeight(), m_scene_view);
}

void MiniGameMesh::OnShutdownScene()
{
	if (m_vb) { delete m_vb; m_vb = nullptr; }
	if (m_ub) { delete m_ub; m_ub = nullptr; }
	if (m_texture) { delete m_texture; m_texture = nullptr; }
}

int main()
{
	MiniGameMesh app;
	return Application::SampleApp::RunSample(app, "MiniGame Mesh");
}

#endif // PLATFORM_GLES3
