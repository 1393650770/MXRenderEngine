#if PLATFORM_GLES3
// MiniGame Mesh Sample — Phase 2 deliverable
// Verifies: VBO + IBO + VAO rendering, SRB uniform buffer, 3D transform.
// Rotating textured cube with procedurally-generated checkerboard texture.

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
#include <cstring>
#include <cmath>
#include <iostream>

using namespace MXRender;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;

// ---- GLSL ES 300 es shaders ----

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

// ---- Cube geometry ----
struct Vertex { float x, y, z, u, v; };

static const Vertex g_cube_verts[] = {
    // Front face
    {-1, 1, 1, 0,1}, {-1,-1, 1, 0,0}, { 1,-1, 1, 1,0},
    { 1, 1, 1, 1,1}, {-1, 1, 1, 0,1}, { 1,-1, 1, 1,0},
    // Back face
    { 1, 1,-1, 0,1}, { 1,-1,-1, 0,0}, {-1,-1,-1, 1,0},
    {-1, 1,-1, 1,1}, { 1, 1,-1, 0,1}, {-1,-1,-1, 1,0},
    // Top face
    {-1, 1,-1, 0,1}, {-1, 1, 1, 0,0}, { 1, 1, 1, 1,0},
    { 1, 1,-1, 1,1}, {-1, 1,-1, 0,1}, { 1, 1, 1, 1,0},
    // Bottom face
    {-1,-1, 1, 0,1}, {-1,-1,-1, 0,0}, { 1,-1,-1, 1,0},
    { 1,-1, 1, 1,1}, {-1,-1, 1, 0,1}, { 1,-1,-1, 1,0},
    // Right face
    { 1, 1, 1, 0,1}, { 1,-1, 1, 0,0}, { 1,-1,-1, 1,0},
    { 1, 1,-1, 1,1}, { 1, 1, 1, 0,1}, { 1,-1,-1, 1,0},
    // Left face
    {-1, 1,-1, 0,1}, {-1,-1,-1, 0,0}, {-1,-1, 1, 1,0},
    {-1, 1, 1, 1,1}, {-1, 1,-1, 0,1}, {-1,-1, 1, 1,0},
};

// MVP uniform buffer struct (std140 layout)
struct MVPBlock
{
	float mvp[16];  // column-major mat4
};

// ---- Pass data ----
struct MeshPassData : public RenderGraphPassDataBase
{
	RenderPipelineState* pso = nullptr;
	ShaderResourceBinding* srb = nullptr;
	VIRTUAL ~MeshPassData() { Release(); }
	void Release()
	{
		if (srb) { delete srb; srb = nullptr; }
	}
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

	GLES3::GLES3_Buffer* m_vb = nullptr;
	GLES3::GLES3_Texture* m_texture = nullptr;
	GLES3::GLES3_Buffer* m_ub = nullptr;
	float m_angle = 0.0f;
MYRENDERER_END_CLASS

// Simple lookAt + perspective matrix (avoiding glm dependency for standalone wasm)
static void BuildMVP(float* out, float angle, float aspect)
{
	// Identity + rotation around Y
	float c = cosf(angle), s = sinf(angle);
	float rot[16] = {c,0,s,0, 0,1,0,0, -s,0,c,0, 0,0,0,1};

	// Perspective proj: fov 60, near 1, far 100
	float f = 1.0f / tanf(1.0472f * 0.5f);
	float proj[16] = {
		f/aspect,0,0,0,
		0,f,0,0,
		0,0,(100+1)/(1-100),-1,
		0,0,(2*100*1)/(1-100),0
	};

	// View: camera at (0,0,6) looking at origin
	float view[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,-6,1};

	// MVP = proj * view * rot (column-major multiply)
	float t[16];
	for (int i=0;i<4;++i) for (int j=0;j<4;++j) {
		t[i+j*4] = 0;
		for (int k=0;k<4;++k) t[i+j*4] += view[i+k*4] * rot[k+j*4];
	}
	for (int i=0;i<4;++i) for (int j=0;j<4;++j) {
		out[i+j*4] = 0;
		for (int k=0;k<4;++k) out[i+j*4] += proj[i+k*4] * t[k+j*4];
	}
}

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

	// ---- Vertex buffer ----
	BufferDesc vb_desc;
	vb_desc.size = sizeof(g_cube_verts);
	vb_desc.type = ENUM_BUFFER_TYPE::Vertex;
	m_vb = static_cast<GLES3::GLES3_Buffer*>(g_render_rhi->CreateBuffer(vb_desc));
	m_vb->SetData(g_cube_verts, sizeof(g_cube_verts));

	// ---- Uniform buffer (MVP matrix) ----
	BufferDesc ub_desc;
	ub_desc.size = sizeof(MVPBlock);
	ub_desc.type = ENUM_BUFFER_TYPE::Uniform;
	m_ub = static_cast<GLES3::GLES3_Buffer*>(g_render_rhi->CreateBuffer(ub_desc));

	// ---- Texture (64x64 checkerboard) ----
	const UInt32 tw = 64, th = 64;
	Vector<UInt32> pixels(tw * th);
	BuildCheckerboard(pixels.data(), tw, th, 8);

	TextureDesc tex_desc;
	tex_desc.width = tw; tex_desc.height = th;
	tex_desc.format = ENUM_TEXTURE_FORMAT::RGBA8;
	tex_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	tex_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
	m_texture = static_cast<GLES3::GLES3_Texture*>(g_render_rhi->CreateTexture(tex_desc));
	GLuint gl_tex = m_texture->GetGLTexture();
	if (gl_tex) {
		glBindTexture(GL_TEXTURE_2D, gl_tex);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)tw, (GLsizei)th,
			GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// ---- Shaders ----
	ShaderDesc vs_desc; vs_desc.shader_type = ENUM_SHADER_STAGE::Shader_Vertex;
	vs_desc.debug_name = "mesh_vs";
	ShaderDataPayload vs_payload; vs_payload.wgsl_source = g_mesh_vs;
	Shader* vs = RHICreateShader(vs_desc, vs_payload);
	static_cast<GLES3::GLES3_Shader*>(vs)->SetGLSLSource(g_mesh_vs);

	ShaderDesc fs_desc; fs_desc.shader_type = ENUM_SHADER_STAGE::Shader_Pixel;
	fs_desc.debug_name = "mesh_fs";
	ShaderDataPayload fs_payload; fs_payload.wgsl_source = g_mesh_fs;
	Shader* fs = RHICreateShader(fs_desc, fs_payload);
	static_cast<GLES3::GLES3_Shader*>(fs)->SetGLSLSource(g_mesh_fs);

	// ---- RenderGraph pass ----
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

			// Update MVP uniform buffer each frame
			MVPBlock mvp;
			BuildMVP(mvp.mvp, m_angle, 1.0f);
			m_ub->SetData(&mvp, sizeof(MVPBlock));

			in_cmd_list->SetGraphicsPipeline(data.pso);
			in_cmd_list->SetShaderResourceBinding(data.srb);
			in_cmd_list->SetVertexBuffer(m_vb, 0, sizeof(Vertex), 0);

			DrawAttribute draw_attr;
			draw_attr.vertexCount = 36;  // 6 faces x 6 vertices (triangle list)
			draw_attr.instanceCount = 1;
			in_cmd_list->Draw(draw_attr);
		});

	rdg_pass->SetIsCullable(false);
	rdg_pass->SetShaderPath("minigame/mesh");
	rdg_pass->SetVertexCount(36);
}

void MiniGameMesh::OnUpdate(Float32 dt)
{
	m_angle += dt * 1.5f;  // ~90 degrees/sec
	if (m_angle > 6.283185f) m_angle -= 6.283185f;
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
