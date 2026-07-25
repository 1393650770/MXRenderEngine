#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <fstream>
#include <cstring>
#include <limits>
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
#include "RHI/RenderBuffer.h"
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;
using namespace MXRender::RHI::Vulkan;
using namespace MXRender;

// -------- FLIP simulation constants (must match the GLSL constants) --------
// Bay domain: x[-3,3], y[0,3], z[-2,2], MAC grid cell 0.1m.
static CONST UInt32 GRID_X = 60;
static CONST UInt32 GRID_Y = 30;
static CONST UInt32 GRID_Z = 40;
static CONST UInt32 NUM_CELLS = GRID_X * GRID_Y * GRID_Z;   // 72000
static CONST UInt32 NUM_V = GRID_X * (GRID_Y + 1) * GRID_Z; // 74400
static CONST UInt32 NUM_FACES = NUM_V;                      // max of U/V/W, uniform allocation
// Particle budget is bounded by DOMAIN VOLUME (8/cell target, 16/cell cull cap),
// not by the poured amount: pouring can continue forever.
static CONST UInt32 MAX_PARTICLES = 786432;
static CONST UInt32 LOCAL_SIZE = 256;
static CONST UInt32 EMIT_PER_FRAME = 32;
static CONST Int SUBSTEPS = 2;
static CONST Int JACOBI_ITERS = 64;    // must be even: pressure ends in p_a
// initial calm sea: lattice at 0.05 spacing up to y = 1.0 (120 x 80 x 20)
static CONST UInt32 PREFILL_X = 120;
static CONST UInt32 PREFILL_Z = 80;
static CONST UInt32 PREFILL_LAYERS = 20;
static CONST UInt32 PREFILL_COUNT = PREFILL_X * PREFILL_Z * PREFILL_LAYERS; // 192000
// world / interaction
static CONST Float32 BAY_X = 3.0f;
static CONST Float32 BAY_Z = 2.0f;
static CONST Float32 WATER_REF_Y = 1.0f;       // aim / interaction reference plane
static CONST Float32 GRAVITY = 9.8f;
static CONST Float32 JET_SPEED = 3.0f;
// screen-space water buffers (fixed internal resolution, 4:3 like the window)
static CONST UInt32 IRES_W = 640;
static CONST UInt32 IRES_H = 480;

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

static Shader* LoadShaderFile(ENUM_SHADER_STAGE stage, CONST String& filename)
{
	ShaderDesc desc;
	desc.shader_type = stage;
	desc.entry_name = "main";
	desc.shader_name = filename;
	ShaderDataPayload payload;
	payload.data = ReadShader(filename);
	return g_render_rhi->CreateShader(desc, payload);
}

static RenderPipelineState* CreateComputePSO(Shader* cs)
{
	RenderGraphiPipelineStateDesc desc{};
	desc.shaders[ENUM_SHADER_STAGE::Shader_Compute] = cs;
	desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
	desc.raster_state.sample_count = 1;
	return g_render_rhi->CreateRenderPipelineState(desc);
}

static Buffer* CreateStorageBuffer(UInt32 element_count, ENUM_BUFFER_TYPE type = ENUM_BUFFER_TYPE::Storage)
{
	BufferDesc desc;
	desc.type = type;
	desc.size = (UInt32)(element_count * sizeof(Float32));
	desc.stride = (UInt32)(element_count * sizeof(Float32));
	Buffer* buffer = g_render_rhi->CreateBuffer(desc);
	void* ptr = RHIMapBuffer(buffer, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::None);
	std::memset(ptr, 0, element_count * sizeof(Float32));
	RHIUnmapBuffer(buffer);
	return buffer;
}

static void UploadFloats(Buffer* buffer, CONST Float32* data, UInt32 count)
{
	void* ptr = RHIMapBuffer(buffer, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::None);
	std::memcpy(ptr, data, count * sizeof(Float32));
	RHIUnmapBuffer(buffer);
}

static void UploadUInts(Buffer* buffer, CONST UInt32* data, UInt32 count)
{
	void* ptr = RHIMapBuffer(buffer, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::None);
	std::memcpy(ptr, data, count * sizeof(UInt32));
	RHIUnmapBuffer(buffer);
}

struct Fluid3DData : public RenderGraphPassDataBase
{
	VIRTUAL ~Fluid3DData() MYDEFAULT;
	VIRTUAL void METHOD(Release)() OVERRIDE {}
};

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
	void METHOD(CreateFluidBuffers)();
	void METHOD(CreateFluidPipelines)();
	void METHOD(CreateFluidBindings)();
	void METHOD(RecordSim)(RHI::CommandList* in_cmd_list);
	void METHOD(RecordClear)(RHI::CommandList* in_cmd_list);
	void METHOD(RecordSplat)(RHI::CommandList* in_cmd_list);
	void METHOD(RecordBlurH)(RHI::CommandList* in_cmd_list);
	void METHOD(RecordBlurV)(RHI::CommandList* in_cmd_list);
	void METHOD(RecordDisplay)(RHI::CommandList* in_cmd_list);
private:

#pragma endregion

#pragma region MEMBER
public:

protected:
	PlatformWindow* m_window = nullptr;

	// particle buffers (vec4 packed as float arrays; pos.w = alive, vel.w = foam)
	RHI::Buffer* pos_buf = nullptr;
	RHI::Buffer* vel_buf = nullptr;
	RHI::Buffer* rank_buf = nullptr;    // P2G cell ticket (uint), cull rank
	RHI::Buffer* free_slots = nullptr;  // free-list ring (uint)
	RHI::Buffer* free_cnt = nullptr;    // {push, pop, actual_emit, base_pop} (uint)
	// MAC grid buffers (all sized NUM_FACES for uniformity)
	RHI::Buffer* mom_u = nullptr;       // fixed-point momentum accumulators (int)
	RHI::Buffer* mom_v = nullptr;
	RHI::Buffer* mom_w = nullptr;
	RHI::Buffer* wgt_u = nullptr;       // fixed-point weight accumulators (int)
	RHI::Buffer* wgt_v = nullptr;
	RHI::Buffer* wgt_w = nullptr;
	RHI::Buffer* grid_u = nullptr;      // face velocities (float)
	RHI::Buffer* grid_v = nullptr;
	RHI::Buffer* grid_w = nullptr;
	RHI::Buffer* old_u = nullptr;       // pre-force snapshot for the FLIP delta
	RHI::Buffer* old_v = nullptr;
	RHI::Buffer* old_w = nullptr;
	RHI::Buffer* cell_count = nullptr;  // particles per cell (uint, atomic)
	RHI::Buffer* div_buf = nullptr;
	RHI::Buffer* p_a = nullptr;         // pressure ping-pong
	RHI::Buffer* p_b = nullptr;
	RHI::Buffer* fp_buf = nullptr;      // per-frame params (64 floats)

	// pipelines (owned by the pipeline state manager, not deleted here)
	RHI::RenderPipelineState* pso_prefill = nullptr;
	RHI::RenderPipelineState* pso_emit_prep = nullptr;
	RHI::RenderPipelineState* pso_emit = nullptr;
	RHI::RenderPipelineState* pso_grid_clear = nullptr;
	RHI::RenderPipelineState* pso_p2g = nullptr;
	RHI::RenderPipelineState* pso_grid_vel = nullptr;
	RHI::RenderPipelineState* pso_divergence = nullptr;
	RHI::RenderPipelineState* pso_jacobi = nullptr;
	RHI::RenderPipelineState* pso_project = nullptr;
	RHI::RenderPipelineState* pso_g2p = nullptr;
	RHI::RenderPipelineState* pso_cull = nullptr;
	RHI::RenderPipelineState* pso_splat = nullptr;
	RHI::RenderPipelineState* pso_blur_h = nullptr;
	RHI::RenderPipelineState* pso_blur_v = nullptr;
	RHI::RenderPipelineState* pso_display = nullptr;

	// shader resource bindings: ALL bound once at init (descriptor sets are
	// referenced by in-flight command buffers; runtime SetResource races)
	RHI::ShaderResourceBinding* srb_prefill = nullptr;
	RHI::ShaderResourceBinding* srb_emit_prep = nullptr;
	RHI::ShaderResourceBinding* srb_emit = nullptr;
	RHI::ShaderResourceBinding* srb_grid_clear = nullptr;
	RHI::ShaderResourceBinding* srb_p2g = nullptr;
	RHI::ShaderResourceBinding* srb_grid_vel = nullptr;
	RHI::ShaderResourceBinding* srb_divergence = nullptr;
	RHI::ShaderResourceBinding* srb_jacobi[2] = { nullptr, nullptr }; // [0] a->b, [1] b->a
	RHI::ShaderResourceBinding* srb_project = nullptr;
	RHI::ShaderResourceBinding* srb_g2p = nullptr;
	RHI::ShaderResourceBinding* srb_cull = nullptr;
	RHI::ShaderResourceBinding* srb_splat = nullptr;
	RHI::ShaderResourceBinding* srb_blur_h = nullptr;
	RHI::ShaderResourceBinding* srb_blur_v = nullptr;
	RHI::ShaderResourceBinding* srb_display = nullptr;

	// retained screen-space textures (created at init, see SRB comment)
	RHI::Texture* ink_depth_u = nullptr; // R32U, view-depth bits (imageAtomicMin)
	RHI::Texture* ink_thick_u = nullptr; // R32U, fixed-point thickness (imageAtomicAdd)
	RHI::Texture* ink_foam_u = nullptr;  // R32U, fixed-point foam (imageAtomicAdd)
	RHI::Texture* ink_f_a = nullptr;     // RGBA16F, blur ping (r=depth g=thickness b=foam)
	RHI::Texture* ink_f_b = nullptr;     // RGBA16F, blur pong, sampled by display

	// per-frame CPU state
	UInt32 frame_index = 0;
	Bool firing = false;
	Bool interacting = false;
	glm::vec3 nozzle_pos = glm::vec3(0.0f, 2.7f, 1.5f);
	glm::vec3 emit_vel = glm::vec3(0.0f, 0.0f, -3.0f);
	glm::vec3 aim_target = glm::vec3(0.0f, WATER_REF_Y, 0.0f);
	glm::vec3 interact_pos = glm::vec3(0.0f);
	Float32 cur_dt = 1.0f / 60.0f;
	Float32 time_sec = 0.0f;
	// perched near the rim so the view goes INTO the bay, not across the deck
	glm::vec3 cam_eye = glm::vec3(0.0f, 6.0f, 3.8f);
	glm::mat4 view_proj = glm::mat4(1.0f);
	glm::mat4 inv_view_proj = glm::mat4(1.0f);
private:

#pragma endregion

MYRENDERER_END_CLASS

void RenderTest::CreateFluidBuffers()
{
	pos_buf = CreateStorageBuffer(MAX_PARTICLES * 4);
	vel_buf = CreateStorageBuffer(MAX_PARTICLES * 4);
	rank_buf = CreateStorageBuffer(MAX_PARTICLES);
	free_slots = CreateStorageBuffer(MAX_PARTICLES);
	free_cnt = CreateStorageBuffer(4);
	mom_u = CreateStorageBuffer(NUM_FACES);
	mom_v = CreateStorageBuffer(NUM_FACES);
	mom_w = CreateStorageBuffer(NUM_FACES);
	wgt_u = CreateStorageBuffer(NUM_FACES);
	wgt_v = CreateStorageBuffer(NUM_FACES);
	wgt_w = CreateStorageBuffer(NUM_FACES);
	grid_u = CreateStorageBuffer(NUM_FACES);
	grid_v = CreateStorageBuffer(NUM_FACES);
	grid_w = CreateStorageBuffer(NUM_FACES);
	old_u = CreateStorageBuffer(NUM_FACES);
	old_v = CreateStorageBuffer(NUM_FACES);
	old_w = CreateStorageBuffer(NUM_FACES);
	cell_count = CreateStorageBuffer(NUM_CELLS);
	div_buf = CreateStorageBuffer(NUM_CELLS);
	p_a = CreateStorageBuffer(NUM_CELLS);
	p_b = CreateStorageBuffer(NUM_CELLS);
	// per-frame params: host-visible (Dynamic) so uploads are direct memory
	// writes; the staging+transfer-queue path is not reliable per frame
	fp_buf = CreateStorageBuffer(64, ENUM_BUFFER_TYPE::Storage | ENUM_BUFFER_TYPE::Dynamic);

	// screen-space textures (retained; storage image support added earlier)
	RHI::TextureDesc ink_uint_desc{};
	ink_uint_desc.width = IRES_W;
	ink_uint_desc.height = IRES_H;
	ink_uint_desc.format = ENUM_TEXTURE_FORMAT::R32U;
	ink_uint_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	ink_uint_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_STORAGE;

	RHI::TextureDesc ink_float_desc{};
	ink_float_desc.width = IRES_W;
	ink_float_desc.height = IRES_H;
	ink_float_desc.format = ENUM_TEXTURE_FORMAT::RGBA16F;
	ink_float_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	ink_float_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_STORAGE | ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;

	ink_depth_u = g_render_rhi->CreateTexture(ink_uint_desc);
	ink_thick_u = g_render_rhi->CreateTexture(ink_uint_desc);
	ink_foam_u = g_render_rhi->CreateTexture(ink_uint_desc);
	ink_f_a = g_render_rhi->CreateTexture(ink_float_desc);
	ink_f_b = g_render_rhi->CreateTexture(ink_float_desc);

	// NOTE: no CPU-side prefill here. Staging uploads to device-local buffers
	// proved unreliable; the fluid3d_prefill.comp pass initializes the sea
	// lattice, free-list and counters on the GPU during the first frame.
}

void RenderTest::CreateFluidPipelines()
{
	// Load every shader first, create every PSO, then delete the shaders.
	// PSO descs are hash-cached by shader pointer value.
	Shader* cs_prefill = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_prefill.comp.spv");
	Shader* cs_emit_prep = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_emit_prep.comp.spv");
	Shader* cs_emit = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_emit.comp.spv");
	Shader* cs_grid_clear = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_grid_clear.comp.spv");
	Shader* cs_p2g = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_p2g.comp.spv");
	Shader* cs_grid_vel = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_grid_vel.comp.spv");
	Shader* cs_divergence = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_divergence.comp.spv");
	Shader* cs_jacobi = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_jacobi.comp.spv");
	Shader* cs_project = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_project.comp.spv");
	Shader* cs_g2p = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_g2p.comp.spv");
	Shader* cs_cull = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_cull.comp.spv");
	Shader* cs_splat = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_splat.comp.spv");
	Shader* cs_blur_h = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_blur_h.comp.spv");
	Shader* cs_blur_v = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid3d_blur_v.comp.spv");
	Shader* vs_fullscreen = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Vertex, "Shader/fullscreen.vert.spv");
	Shader* ps_display = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Pixel, "Shader/fluid3d_display.frag.spv");

	pso_prefill = CreateComputePSO(cs_prefill);
	pso_emit_prep = CreateComputePSO(cs_emit_prep);
	pso_emit = CreateComputePSO(cs_emit);
	pso_grid_clear = CreateComputePSO(cs_grid_clear);
	pso_p2g = CreateComputePSO(cs_p2g);
	pso_grid_vel = CreateComputePSO(cs_grid_vel);
	pso_divergence = CreateComputePSO(cs_divergence);
	pso_jacobi = CreateComputePSO(cs_jacobi);
	pso_project = CreateComputePSO(cs_project);
	pso_g2p = CreateComputePSO(cs_g2p);
	pso_cull = CreateComputePSO(cs_cull);
	pso_splat = CreateComputePSO(cs_splat);
	pso_blur_h = CreateComputePSO(cs_blur_h);
	pso_blur_v = CreateComputePSO(cs_blur_v);

	RenderGraphiPipelineStateDesc display_desc;
	display_desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs_fullscreen;
	display_desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps_display;
	display_desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
	Vector<Texture*> rtvs = { m_viewport->GetCurrentBackBufferRTV() };
	display_desc.render_targets = rtvs;
	display_desc.depth_stencil_view = m_viewport->GetCurrentBackBufferDSV();
	display_desc.raster_state.sample_count = 1;
	display_desc.blend_state.render_targets.resize(rtvs.size());
	pso_display = g_render_rhi->CreateRenderPipelineState(display_desc);

	delete cs_prefill;
	delete cs_emit_prep;
	delete cs_emit;
	delete cs_grid_clear;
	delete cs_p2g;
	delete cs_grid_vel;
	delete cs_divergence;
	delete cs_jacobi;
	delete cs_project;
	delete cs_g2p;
	delete cs_cull;
	delete cs_splat;
	delete cs_blur_h;
	delete cs_blur_v;
	delete vs_fullscreen;
	delete ps_display;
}

void RenderTest::CreateFluidBindings()
{
	pso_prefill->CreateShaderResourceBinding(srb_prefill, false);
	srb_prefill->SetResource("pos", pos_buf);
	srb_prefill->SetResource("vel", vel_buf);
	srb_prefill->SetResource("fslots", free_slots);
	srb_prefill->SetResource("fc", free_cnt);
	srb_prefill->SetResource("pa", p_a);
	srb_prefill->SetResource("pb", p_b);
	srb_prefill->FlushDescriptorWrites();

	pso_emit_prep->CreateShaderResourceBinding(srb_emit_prep, false);
	srb_emit_prep->SetResource("fp", fp_buf);
	srb_emit_prep->SetResource("fc", free_cnt);
	srb_emit_prep->FlushDescriptorWrites();

	pso_emit->CreateShaderResourceBinding(srb_emit, false);
	srb_emit->SetResource("fp", fp_buf);
	srb_emit->SetResource("fc", free_cnt);
	srb_emit->SetResource("fslots", free_slots);
	srb_emit->SetResource("pos", pos_buf);
	srb_emit->SetResource("vel", vel_buf);
	srb_emit->FlushDescriptorWrites();

	pso_grid_clear->CreateShaderResourceBinding(srb_grid_clear, false);
	srb_grid_clear->SetResource("mu", mom_u);
	srb_grid_clear->SetResource("mv", mom_v);
	srb_grid_clear->SetResource("mw", mom_w);
	srb_grid_clear->SetResource("wu", wgt_u);
	srb_grid_clear->SetResource("wv", wgt_v);
	srb_grid_clear->SetResource("ww", wgt_w);
	srb_grid_clear->SetResource("cnt", cell_count);
	srb_grid_clear->FlushDescriptorWrites();

	pso_p2g->CreateShaderResourceBinding(srb_p2g, false);
	srb_p2g->SetResource("fp", fp_buf);
	srb_p2g->SetResource("pos", pos_buf);
	srb_p2g->SetResource("vel", vel_buf);
	srb_p2g->SetResource("mu", mom_u);
	srb_p2g->SetResource("mv", mom_v);
	srb_p2g->SetResource("mw", mom_w);
	srb_p2g->SetResource("wu", wgt_u);
	srb_p2g->SetResource("wv", wgt_v);
	srb_p2g->SetResource("ww", wgt_w);
	srb_p2g->SetResource("cnt", cell_count);
	srb_p2g->SetResource("rank", rank_buf);
	srb_p2g->FlushDescriptorWrites();

	pso_grid_vel->CreateShaderResourceBinding(srb_grid_vel, false);
	srb_grid_vel->SetResource("fp", fp_buf);
	srb_grid_vel->SetResource("mu", mom_u);
	srb_grid_vel->SetResource("mv", mom_v);
	srb_grid_vel->SetResource("mw", mom_w);
	srb_grid_vel->SetResource("wu", wgt_u);
	srb_grid_vel->SetResource("wv", wgt_v);
	srb_grid_vel->SetResource("ww", wgt_w);
	srb_grid_vel->SetResource("gu", grid_u);
	srb_grid_vel->SetResource("gv", grid_v);
	srb_grid_vel->SetResource("gw", grid_w);
	srb_grid_vel->SetResource("ou", old_u);
	srb_grid_vel->SetResource("ov", old_v);
	srb_grid_vel->SetResource("ow", old_w);
	srb_grid_vel->FlushDescriptorWrites();

	pso_divergence->CreateShaderResourceBinding(srb_divergence, false);
	srb_divergence->SetResource("cnt", cell_count);
	srb_divergence->SetResource("gu", grid_u);
	srb_divergence->SetResource("gv", grid_v);
	srb_divergence->SetResource("gw", grid_w);
	srb_divergence->SetResource("dv", div_buf);
	srb_divergence->FlushDescriptorWrites();

	// jacobi ping-pong: [0] a->b, [1] b->a; JACOBI_ITERS even -> result in p_a
	for (Int p = 0; p < 2; ++p)
	{
		pso_jacobi->CreateShaderResourceBinding(srb_jacobi[p], false);
		srb_jacobi[p]->SetResource("cnt", cell_count);
		srb_jacobi[p]->SetResource("dv", div_buf);
		srb_jacobi[p]->SetResource("pin", p == 0 ? p_a : p_b);
		srb_jacobi[p]->SetResource("pout", p == 0 ? p_b : p_a);
		srb_jacobi[p]->FlushDescriptorWrites();
	}

	pso_project->CreateShaderResourceBinding(srb_project, false);
	srb_project->SetResource("cnt", cell_count);
	srb_project->SetResource("pa", p_a);
	srb_project->SetResource("gu", grid_u);
	srb_project->SetResource("gv", grid_v);
	srb_project->SetResource("gw", grid_w);
	srb_project->FlushDescriptorWrites();

	pso_g2p->CreateShaderResourceBinding(srb_g2p, false);
	srb_g2p->SetResource("fp", fp_buf);
	srb_g2p->SetResource("gu", grid_u);
	srb_g2p->SetResource("gv", grid_v);
	srb_g2p->SetResource("gw", grid_w);
	srb_g2p->SetResource("ou", old_u);
	srb_g2p->SetResource("ov", old_v);
	srb_g2p->SetResource("ow", old_w);
	srb_g2p->SetResource("pos", pos_buf);
	srb_g2p->SetResource("vel", vel_buf);
	srb_g2p->FlushDescriptorWrites();

	pso_cull->CreateShaderResourceBinding(srb_cull, false);
	srb_cull->SetResource("pos", pos_buf);
	srb_cull->SetResource("rank", rank_buf);
	srb_cull->SetResource("fslots", free_slots);
	srb_cull->SetResource("fc", free_cnt);
	srb_cull->FlushDescriptorWrites();

	pso_splat->CreateShaderResourceBinding(srb_splat, false);
	srb_splat->SetResource("fp", fp_buf);
	srb_splat->SetResource("pos", pos_buf);
	srb_splat->SetResource("vel", vel_buf);
	srb_splat->SetResource("uimg_depth", ink_depth_u);
	srb_splat->SetResource("uimg_thick", ink_thick_u);
	srb_splat->SetResource("uimg_foam", ink_foam_u);
	srb_splat->FlushDescriptorWrites();

	pso_blur_h->CreateShaderResourceBinding(srb_blur_h, false);
	srb_blur_h->SetResource("uimg_depth", ink_depth_u);
	srb_blur_h->SetResource("uimg_thick", ink_thick_u);
	srb_blur_h->SetResource("uimg_foam", ink_foam_u);
	srb_blur_h->SetResource("img_out", ink_f_a);
	srb_blur_h->FlushDescriptorWrites();

	pso_blur_v->CreateShaderResourceBinding(srb_blur_v, false);
	srb_blur_v->SetResource("tex_in", ink_f_a);
	srb_blur_v->SetResource("img_out", ink_f_b);
	srb_blur_v->FlushDescriptorWrites();

	pso_display->CreateShaderResourceBinding(srb_display, false);
	srb_display->SetResource("fp", fp_buf);
	srb_display->SetResource("ink_tex", ink_f_b);
	srb_display->FlushDescriptorWrites();
}

void RenderTest::RecordSim(RHI::CommandList* in_cmd_list)
{
	CONST UInt32 emit_request = firing ? EMIT_PER_FRAME : 0;

	// fp layout: [0]=substep dt [3]=emit request [4-6]=nozzle [7]=fire
	//            [8-10]=emit_vel [11]=seed [12-14]=interact [15]=interact_flag
	//            [16-31]=view_proj [32-34]=aim [35]=time
	//            [36-51]=inv_view_proj [52-54]=camera eye (matrices column major)
	Float32 params[64] = {};
	params[0] = cur_dt / (Float32)SUBSTEPS;
	params[3] = (Float32)emit_request;
	params[4] = nozzle_pos.x;
	params[5] = nozzle_pos.y;
	params[6] = nozzle_pos.z;
	params[7] = firing ? 1.0f : 0.0f;
	params[8] = emit_vel.x;
	params[9] = emit_vel.y;
	params[10] = emit_vel.z;
	params[11] = (Float32)(frame_index & 0xFFFFu);
	params[12] = interact_pos.x;
	params[13] = interact_pos.y;
	params[14] = interact_pos.z;
	params[15] = interacting ? 1.0f : 0.0f;
	std::memcpy(&params[16], &view_proj[0][0], 16 * sizeof(Float32));
	params[32] = aim_target.x;
	params[33] = aim_target.y;
	params[34] = aim_target.z;
	params[35] = time_sec;
	std::memcpy(&params[36], &inv_view_proj[0][0], 16 * sizeof(Float32));
	params[52] = cam_eye.x;
	params[53] = cam_eye.y;
	params[54] = cam_eye.z;
	UploadFloats(fp_buf, params, 64);


	auto run_compute = [&](RenderPipelineState* pso, ShaderResourceBinding* srb, UInt32 gx)
	{
		in_cmd_list->SetComputePipeline(pso);
		in_cmd_list->SetShaderResourceBinding(srb);
		in_cmd_list->Dispatch(gx, 1, 1);
		// write->read + write->write; accumulated into one pipeline barrier
		// flushed by the next Dispatch
		in_cmd_list->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::ShaderResource);
		in_cmd_list->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::UnorderedAccess);
	};

	CONST UInt32 g_particles = MAX_PARTICLES / LOCAL_SIZE;                    // 3072
	CONST UInt32 g_faces = (NUM_FACES + LOCAL_SIZE - 1) / LOCAL_SIZE;         // 291
	CONST UInt32 g_cells = (NUM_CELLS + LOCAL_SIZE - 1) / LOCAL_SIZE;         // 282

	// one-shot GPU init (first frame): sea lattice + free-list + counters
	if (frame_index == 0)
	{
		run_compute(pso_prefill, srb_prefill, g_particles);
	}

	if (emit_request > 0)
	{
		run_compute(pso_emit_prep, srb_emit_prep, 1);
		run_compute(pso_emit, srb_emit, (emit_request + LOCAL_SIZE - 1) / LOCAL_SIZE);
	}

	for (Int s = 0; s < SUBSTEPS; ++s)
	{
		run_compute(pso_grid_clear, srb_grid_clear, g_faces);
		run_compute(pso_p2g, srb_p2g, g_particles);
		run_compute(pso_grid_vel, srb_grid_vel, g_faces);
		run_compute(pso_divergence, srb_divergence, g_cells);
		for (Int i = 0; i < JACOBI_ITERS; ++i)
		{
			run_compute(pso_jacobi, srb_jacobi[i & 1], g_cells);
		}
		run_compute(pso_project, srb_project, g_faces);
		run_compute(pso_g2p, srb_g2p, g_particles);
		run_compute(pso_cull, srb_cull, g_particles);
	}

	frame_index++;
}

void RenderTest::RecordClear(RHI::CommandList* in_cmd_list)
{
	// frame-head clear via vkCmdClearColorImage (ClearTexture transitions to
	// CopyDest internally). VkClearColorValue is a union: +inf float bits
	// (0x7F800000) are exactly the FAR sentinel imageAtomicMin expects.
	CONST Float32 far_sentinel = std::numeric_limits<Float32>::infinity();
	in_cmd_list->ClearTexture(ink_depth_u, { far_sentinel, 0.0f, 0.0f, 0.0f });
	in_cmd_list->ClearTexture(ink_thick_u, { 0.0f, 0.0f, 0.0f, 0.0f });
	in_cmd_list->ClearTexture(ink_foam_u, { 0.0f, 0.0f, 0.0f, 0.0f });
}

void RenderTest::RecordSplat(RHI::CommandList* in_cmd_list)
{
	// CopyDest (after the clear pass) -> GENERAL for imageAtomic writes
	in_cmd_list->TransitionTextureState(ink_depth_u, ENUM_RESOURCE_STATE::UnorderedAccess);
	in_cmd_list->TransitionTextureState(ink_thick_u, ENUM_RESOURCE_STATE::UnorderedAccess);
	in_cmd_list->TransitionTextureState(ink_foam_u, ENUM_RESOURCE_STATE::UnorderedAccess);

	in_cmd_list->SetComputePipeline(pso_splat);
	in_cmd_list->SetShaderResourceBinding(srb_splat);
	in_cmd_list->Dispatch(MAX_PARTICLES / LOCAL_SIZE, 1, 1);
	in_cmd_list->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::ShaderResource);
	in_cmd_list->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::UnorderedAccess);
}

void RenderTest::RecordBlurH(RHI::CommandList* in_cmd_list)
{
	// frame 0: Undefined -> GENERAL, afterwards SRV -> GENERAL (BlurV read it)
	in_cmd_list->TransitionTextureState(ink_f_a, ENUM_RESOURCE_STATE::UnorderedAccess);

	in_cmd_list->SetComputePipeline(pso_blur_h);
	in_cmd_list->SetShaderResourceBinding(srb_blur_h);
	in_cmd_list->Dispatch((IRES_W + 15) / 16, (IRES_H + 15) / 16, 1);
	in_cmd_list->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::ShaderResource);
	in_cmd_list->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::UnorderedAccess);
}

void RenderTest::RecordBlurV(RHI::CommandList* in_cmd_list)
{
	// f_a: GENERAL -> SHADER_READ_ONLY (sampled via texelFetch)
	in_cmd_list->TransitionTextureState(ink_f_a, ENUM_RESOURCE_STATE::ShaderResource);
	// f_b: frame 0 Undefined -> GENERAL, afterwards SRV -> GENERAL
	in_cmd_list->TransitionTextureState(ink_f_b, ENUM_RESOURCE_STATE::UnorderedAccess);

	in_cmd_list->SetComputePipeline(pso_blur_v);
	in_cmd_list->SetShaderResourceBinding(srb_blur_v);
	in_cmd_list->Dispatch((IRES_W + 15) / 16, (IRES_H + 15) / 16, 1);
	in_cmd_list->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::ShaderResource);
}

void RenderTest::RecordDisplay(RHI::CommandList* in_cmd_list)
{
	// f_b: GENERAL -> SHADER_READ_ONLY for hardware bilinear sampling
	in_cmd_list->TransitionTextureState(ink_f_b, ENUM_RESOURCE_STATE::ShaderResource);

	Vector<ClearValue> clear_values;
	Vector<Texture*> rtvs = { m_viewport->GetCurrentBackBufferRTV() };
	Texture* dsv = m_viewport->GetCurrentBackBufferDSV();
	for (auto rtv : rtvs)
		clear_values.push_back(rtv->GetTextureDesc().clear_value);
	if (dsv)
		clear_values.push_back(dsv->GetTextureDesc().clear_value);
	in_cmd_list->SetRenderTarget(rtvs, dsv, clear_values, dsv != nullptr);
	in_cmd_list->SetGraphicsPipeline(pso_display);
	in_cmd_list->SetShaderResourceBinding(srb_display);
	DrawAttribute draw_attr;
	draw_attr.vertexCount = 3;
	draw_attr.instanceCount = 1;
	in_cmd_list->Draw(draw_attr);
}

void RenderTest::OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport)
{
	m_window = in_window;
	m_viewport = in_viewport;
	std::cout << "Hello Fluid3D (FLIP bay)" << std::endl;

	RHI::CommandList* cmd_list = RHIGetImmediateCommandList();

	// Step 1: Register external resources (BackBuffer + DepthStencil)
	RHI::Texture* backbuffer_rtv = m_viewport->GetCurrentBackBufferRTV();
	RHI::Texture* backbuffer_dsv = m_viewport->GetCurrentBackBufferDSV();

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

	// Step 2: fixed camera overlooking the bay
	glm::mat4 view = glm::lookAt(cam_eye, glm::vec3(0.0f, 0.9f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), (Float32)IRES_W / (Float32)IRES_H, 0.1f, 100.0f);
	view_proj = proj * view;
	inv_view_proj = glm::inverse(view_proj);

	// Step 3: create simulation resources
	CreateFluidBuffers();
	CreateFluidPipelines();
	CreateFluidBindings();

	// Step 4: build the 6-pass graph (retained textures + manual transitions)
	auto* ink_depth_res = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
		"InkDepthU", ink_depth_u->GetTextureDesc(), ink_depth_u);
	auto* ink_thick_res = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
		"InkThickU", ink_thick_u->GetTextureDesc(), ink_thick_u);
	auto* ink_foam_res = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
		"InkFoamU", ink_foam_u->GetTextureDesc(), ink_foam_u);
	auto* ink_f_a_res = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
		"InkBlurA", ink_f_a->GetTextureDesc(), ink_f_a);
	auto* ink_f_b_res = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
		"InkBlurB", ink_f_b->GetTextureDesc(), ink_f_b);

	auto* clear_pass = graph.AddRenderPass<Fluid3DData>("Fluid3DClearPass", &graph, cmd_list,
	[&](Fluid3DData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd_list)
	{
		builder.Write(ink_depth_res, ENUM_RESOURCE_STATE::CopyDest);
		builder.Write(ink_thick_res, ENUM_RESOURCE_STATE::CopyDest);
		builder.Write(ink_foam_res, ENUM_RESOURCE_STATE::CopyDest);
	},
	[=](CONST Fluid3DData& data, CommandList* in_cmd_list)
	{
		this->RecordClear(in_cmd_list);
	});
	clear_pass->SetIsCullable(false);

	auto* sim_pass = graph.AddRenderPass<Fluid3DData>("Fluid3DSimPass", &graph, cmd_list,
	[&](Fluid3DData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd_list)
	{
		// all simulation buffers live outside the graph (manual barriers)
	},
	[=](CONST Fluid3DData& data, CommandList* in_cmd_list)
	{
		this->RecordSim(in_cmd_list);
	});
	sim_pass->SetIsCullable(false);

	auto* splat_pass = graph.AddRenderPass<Fluid3DData>("Fluid3DSplatPass", &graph, cmd_list,
	[&](Fluid3DData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd_list)
	{
		builder.Write(ink_depth_res, ENUM_RESOURCE_STATE::UnorderedAccess);
		builder.Write(ink_thick_res, ENUM_RESOURCE_STATE::UnorderedAccess);
		builder.Write(ink_foam_res, ENUM_RESOURCE_STATE::UnorderedAccess);
	},
	[=](CONST Fluid3DData& data, CommandList* in_cmd_list)
	{
		this->RecordSplat(in_cmd_list);
	});
	splat_pass->SetIsCullable(false);

	auto* blur_h_pass = graph.AddRenderPass<Fluid3DData>("Fluid3DBlurHPass", &graph, cmd_list,
	[&](Fluid3DData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd_list)
	{
		builder.Read(ink_depth_res, ENUM_RESOURCE_STATE::UnorderedAccess);
		builder.Read(ink_thick_res, ENUM_RESOURCE_STATE::UnorderedAccess);
		builder.Read(ink_foam_res, ENUM_RESOURCE_STATE::UnorderedAccess);
		builder.Write(ink_f_a_res, ENUM_RESOURCE_STATE::UnorderedAccess);
	},
	[=](CONST Fluid3DData& data, CommandList* in_cmd_list)
	{
		this->RecordBlurH(in_cmd_list);
	});
	blur_h_pass->SetIsCullable(false);

	auto* blur_v_pass = graph.AddRenderPass<Fluid3DData>("Fluid3DBlurVPass", &graph, cmd_list,
	[&](Fluid3DData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd_list)
	{
		builder.Read(ink_f_a_res, ENUM_RESOURCE_STATE::ShaderResource);
		builder.Write(ink_f_b_res, ENUM_RESOURCE_STATE::UnorderedAccess);
	},
	[=](CONST Fluid3DData& data, CommandList* in_cmd_list)
	{
		this->RecordBlurV(in_cmd_list);
	});
	blur_v_pass->SetIsCullable(false);

	auto* display_pass = graph.AddRenderPass<Fluid3DData>("Fluid3DDisplayPass", &graph, cmd_list,
	[&](Fluid3DData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd_list)
	{
		builder.Read(ink_f_b_res, ENUM_RESOURCE_STATE::ShaderResource);
		builder.Write(rt_resource);
		if (ds_resource) builder.Write(ds_resource);
	},
	[=](CONST Fluid3DData& data, CommandList* in_cmd_list)
	{
		this->RecordDisplay(in_cmd_list);
	});
	display_pass->SetIsCullable(false);

	graph.Compile();
}

void RenderTest::OnShutdown_Logic()
{
	graph.Release();

	RHI::ShaderResourceBinding* srbs[] = {
		srb_prefill, srb_emit_prep, srb_emit, srb_grid_clear, srb_p2g, srb_grid_vel,
		srb_divergence, srb_jacobi[0], srb_jacobi[1], srb_project,
		srb_g2p, srb_cull, srb_splat, srb_blur_h, srb_blur_v, srb_display };
	for (auto* srb : srbs)
	{
		delete srb;
	}

	RHI::Buffer* buffers[] = {
		pos_buf, vel_buf, rank_buf, free_slots, free_cnt,
		mom_u, mom_v, mom_w, wgt_u, wgt_v, wgt_w,
		grid_u, grid_v, grid_w, old_u, old_v, old_w,
		cell_count, div_buf, p_a, p_b, fp_buf };
	for (auto* buf : buffers)
	{
		delete buf;
	}

	RHI::Texture* textures[] = { ink_depth_u, ink_thick_u, ink_foam_u, ink_f_a, ink_f_b };
	for (auto* tex : textures)
	{
		delete tex;
	}
}

void RenderTest::OnUpdate(float dt)
{
	PlatformWindow* pw = GetPlatformWindow();
	int win_w = 0, win_h = 0;
	pw->GetFramebufferSize(win_w, win_h);
	Float64 cx = 0.0, cy = 0.0;
	pw->GetCursorPos(cx, cy);
	firing = pw->GetMouseButton(MXRender::MouseButton::Left);
	interacting = pw->GetMouseButton(MXRender::MouseButton::Right);

	if (win_w <= 0 || win_h <= 0)
	{
		firing = false;
		interacting = false;
		return;
	}

	// cursor -> NDC -> world ray -> hit on the aim plane y = WATER_REF_Y
	Float32 nx = (Float32)(2.0 * cx / (Float64)win_w - 1.0);
	Float32 ny = (Float32)(1.0 - 2.0 * cy / (Float64)win_h);
	glm::vec4 p0h = inv_view_proj * glm::vec4(nx, ny, -1.0f, 1.0f);
	glm::vec4 p1h = inv_view_proj * glm::vec4(nx, ny, 1.0f, 1.0f);
	glm::vec3 p0 = glm::vec3(p0h) / p0h.w;
	glm::vec3 p1 = glm::vec3(p1h) / p1h.w;
	glm::vec3 ray_dir = glm::normalize(p1 - p0);
	if (ray_dir.y < -1e-4f)
	{
		Float32 t = (WATER_REF_Y - p0.y) / ray_dir.y;
		glm::vec3 hit = p0 + ray_dir * t;
		hit.x = glm::clamp(hit.x, -(BAY_X - 0.2f), BAY_X - 0.2f);
		hit.z = glm::clamp(hit.z, -(BAY_Z - 0.2f), BAY_Z - 0.2f);
		hit.y = WATER_REF_Y;
		aim_target = hit;
	}

	// nozzle hovers over the near side (inside the bay), x slides with the aim
	nozzle_pos = glm::vec3(glm::clamp(aim_target.x * 0.5f, -2.4f, 2.4f), 2.7f, 1.5f);

	// ballistic solve so the jet lands on the aim point
	glm::vec3 delta = aim_target - nozzle_pos;
	Float32 flight_t = glm::clamp(glm::length(delta) / JET_SPEED, 0.3f, 1.0f);
	emit_vel = delta / flight_t;
	emit_vel.y += 0.5f * GRAVITY * flight_t;

	interact_pos = aim_target;

	cur_dt = glm::clamp(dt, 1.0f / 240.0f, 1.0f / 60.0f);
	time_sec += dt;
}

void RenderTest::OnRender()
{
	graph.Execute();
}

RenderTest::RenderTest(PlatformWindow* in_window) :m_window(in_window)
{

}

MXRender::PlatformWindow* RenderTest::GetPlatformWindow()
{
	return m_window;
}

int main()
{
	Window window;
	RenderTest render(window.GetPlatformWindow());
	window.InitWindow();
	window.Run(&render);
	system("pause");

	return 0;
}



