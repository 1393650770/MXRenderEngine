#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
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

// -------- simulation constants (grid size must match the GLSL constants) --------
static CONST UInt32 FLUID_W = 512;
static CONST UInt32 FLUID_H = 384;
static CONST UInt32 GROUP_COUNT_X = FLUID_W / 16;
static CONST UInt32 GROUP_COUNT_Y = FLUID_H / 16;
static CONST Int JACOBI_ITERS = 24; // must be even: final pressure always lands in p_a (warm start)
static CONST Float32 SPLAT_RADIUS = 12.0f;   // in grid cells
static CONST Float32 DYE_AMOUNT = 0.25f;     // dye injected per frame while dragging
static CONST Float32 FORCE_SCALE = 60.0f;    // mouse move (grid cells/frame) -> velocity
static CONST Float32 BLUR_STRIDE = 2.0f;     // gaussian tap stride, widens the metaball merge
static CONST Float32 MAX_DT = 1.0f / 30.0f;  // clamp dt to keep advection stable

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

static Buffer* CreateStorageBuffer(UInt32 float_count)
{
	BufferDesc desc;
	desc.type = ENUM_BUFFER_TYPE::Storage;
	desc.size = (UInt32)(float_count * sizeof(Float32));
	desc.stride = (UInt32)(float_count * sizeof(Float32));
	Buffer* buffer = g_render_rhi->CreateBuffer(desc);
	void* ptr = RHIMapBuffer(buffer, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::None);
	std::memset(ptr, 0, float_count * sizeof(Float32));
	RHIUnmapBuffer(buffer);
	return buffer;
}

static void UploadFloats(Buffer* buffer, CONST Float32* data, UInt32 count)
{
	void* ptr = RHIMapBuffer(buffer, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::None);
	std::memcpy(ptr, data, count * sizeof(Float32));
	RHIUnmapBuffer(buffer);
}

struct FluidData : public RenderGraphPassDataBase
{
	VIRTUAL ~FluidData() MYDEFAULT;
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
	void METHOD(RecordFrame)(RHI::CommandList* in_cmd_list);
private:

#pragma endregion

#pragma region MEMBER
public:

protected:
	PlatformWindow* m_window = nullptr;

	// field storage buffers (y*W+x indexed float arrays)
	RHI::Buffer* vel_a = nullptr;    // velocity, vec2 per cell
	RHI::Buffer* vel_b = nullptr;
	RHI::Buffer* p_a = nullptr;      // pressure ping-pong
	RHI::Buffer* p_b = nullptr;
	RHI::Buffer* div_buf = nullptr;  // divergence
	RHI::Buffer* dye_ab[2] = { nullptr, nullptr }; // dye, frame parity ping-pong
	RHI::Buffer* blur_tmp = nullptr;
	RHI::Buffer* blur_out = nullptr;
	RHI::Buffer* fp_buf = nullptr;   // per-frame params
	RHI::Buffer* bdir_h = nullptr;   // blur direction params (uploaded once)
	RHI::Buffer* bdir_v = nullptr;

	// pipelines (owned by the pipeline state manager, not deleted here)
	RHI::RenderPipelineState* pso_splat = nullptr;
	RHI::RenderPipelineState* pso_advect_vel = nullptr;
	RHI::RenderPipelineState* pso_div = nullptr;
	RHI::RenderPipelineState* pso_jacobi = nullptr;
	RHI::RenderPipelineState* pso_project = nullptr;
	RHI::RenderPipelineState* pso_advect_dye = nullptr;
	RHI::RenderPipelineState* pso_blur = nullptr;
	RHI::RenderPipelineState* pso_display = nullptr;

	// shader resource bindings (bound once at init, selected per frame parity)
	RHI::ShaderResourceBinding* srb_splat[2] = { nullptr, nullptr };
	RHI::ShaderResourceBinding* srb_advect_vel = nullptr;
	RHI::ShaderResourceBinding* srb_div = nullptr;
	RHI::ShaderResourceBinding* srb_jacobi[2] = { nullptr, nullptr };
	RHI::ShaderResourceBinding* srb_project = nullptr;
	RHI::ShaderResourceBinding* srb_advect_dye[2] = { nullptr, nullptr };
	RHI::ShaderResourceBinding* srb_blur_h[2] = { nullptr, nullptr };
	RHI::ShaderResourceBinding* srb_blur_v = nullptr;
	RHI::ShaderResourceBinding* srb_display[2] = { nullptr, nullptr };

	// per-frame simulation state
	Int frame_parity = 0;
	Bool mouse_down = false;
	Bool is_radial = false;
	Float32 mouse_gx = 0.0f;
	Float32 mouse_gy = 0.0f;
	Float32 force_x = 0.0f;
	Float32 force_y = 0.0f;
	Float64 last_cursor_x = 0.0;
	Float64 last_cursor_y = 0.0;
	Bool has_last_cursor = false;
	Float32 cur_dt = 1.0f / 60.0f;
private:

#pragma endregion

MYRENDERER_END_CLASS

void RenderTest::CreateFluidBuffers()
{
	vel_a = CreateStorageBuffer(FLUID_W * FLUID_H * 2);
	vel_b = CreateStorageBuffer(FLUID_W * FLUID_H * 2);
	p_a = CreateStorageBuffer(FLUID_W * FLUID_H);
	p_b = CreateStorageBuffer(FLUID_W * FLUID_H);
	div_buf = CreateStorageBuffer(FLUID_W * FLUID_H);
	dye_ab[0] = CreateStorageBuffer(FLUID_W * FLUID_H);
	dye_ab[1] = CreateStorageBuffer(FLUID_W * FLUID_H);
	blur_tmp = CreateStorageBuffer(FLUID_W * FLUID_H);
	blur_out = CreateStorageBuffer(FLUID_W * FLUID_H);
	fp_buf = CreateStorageBuffer(12);
	bdir_h = CreateStorageBuffer(4);
	bdir_v = CreateStorageBuffer(4);

	Float32 dir_h[4] = { 1.0f, 0.0f, BLUR_STRIDE, 0.0f };
	Float32 dir_v[4] = { 0.0f, 1.0f, BLUR_STRIDE, 0.0f };
	UploadFloats(bdir_h, dir_h, 4);
	UploadFloats(bdir_v, dir_v, 4);
}

void RenderTest::CreateFluidPipelines()
{
	// Load every shader first, create every PSO, then delete the shaders.
	// PSO descs are hash-cached by shader pointer value: interleaving
	// create/delete could recycle a heap address and collide two pipelines.
	Shader* cs_splat = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid_splat.comp.spv");
	Shader* cs_advect_vel = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid_advect_vel.comp.spv");
	Shader* cs_div = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid_divergence.comp.spv");
	Shader* cs_jacobi = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid_jacobi.comp.spv");
	Shader* cs_project = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid_project.comp.spv");
	Shader* cs_advect_dye = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid_advect_dye.comp.spv");
	Shader* cs_blur = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Compute, "Shader/fluid_blur.comp.spv");
	Shader* vs_fullscreen = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Vertex, "Shader/fullscreen.vert.spv");
	Shader* ps_display = LoadShaderFile(ENUM_SHADER_STAGE::Shader_Pixel, "Shader/fluid_display.frag.spv");

	pso_splat = CreateComputePSO(cs_splat);
	pso_advect_vel = CreateComputePSO(cs_advect_vel);
	pso_div = CreateComputePSO(cs_div);
	pso_jacobi = CreateComputePSO(cs_jacobi);
	pso_project = CreateComputePSO(cs_project);
	pso_advect_dye = CreateComputePSO(cs_advect_dye);
	pso_blur = CreateComputePSO(cs_blur);

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

	delete cs_splat;
	delete cs_advect_vel;
	delete cs_div;
	delete cs_jacobi;
	delete cs_project;
	delete cs_advect_dye;
	delete cs_blur;
	delete vs_fullscreen;
	delete ps_display;
}

void RenderTest::CreateFluidBindings()
{
	// splat: in-place force + dye injection (per dye parity)
	for (Int p = 0; p < 2; ++p)
	{
		pso_splat->CreateShaderResourceBinding(srb_splat[p], false);
		srb_splat[p]->SetResource("fp", fp_buf);
		srb_splat[p]->SetResource("v0", vel_a);
		srb_splat[p]->SetResource("d0", dye_ab[p]);
		srb_splat[p]->FlushDescriptorWrites();
	}

	pso_advect_vel->CreateShaderResourceBinding(srb_advect_vel, false);
	srb_advect_vel->SetResource("fp", fp_buf);
	srb_advect_vel->SetResource("vin", vel_a);
	srb_advect_vel->SetResource("vout", vel_b);
	srb_advect_vel->FlushDescriptorWrites();

	pso_div->CreateShaderResourceBinding(srb_div, false);
	srb_div->SetResource("vin", vel_b);
	srb_div->SetResource("dout", div_buf);
	srb_div->FlushDescriptorWrites();

	// jacobi pressure ping-pong: [0] p_a->p_b, [1] p_b->p_a
	for (Int p = 0; p < 2; ++p)
	{
		pso_jacobi->CreateShaderResourceBinding(srb_jacobi[p], false);
		srb_jacobi[p]->SetResource("pin", p == 0 ? p_a : p_b);
		srb_jacobi[p]->SetResource("dv", div_buf);
		srb_jacobi[p]->SetResource("pout", p == 0 ? p_b : p_a);
		srb_jacobi[p]->FlushDescriptorWrites();
	}

	pso_project->CreateShaderResourceBinding(srb_project, false);
	srb_project->SetResource("pin", p_a);
	srb_project->SetResource("vin", vel_b);
	srb_project->SetResource("vout", vel_a);
	srb_project->FlushDescriptorWrites();

	// dye advection: dye[p] -> dye[1-p]
	for (Int p = 0; p < 2; ++p)
	{
		pso_advect_dye->CreateShaderResourceBinding(srb_advect_dye[p], false);
		srb_advect_dye[p]->SetResource("fp", fp_buf);
		srb_advect_dye[p]->SetResource("vin", vel_a);
		srb_advect_dye[p]->SetResource("din", dye_ab[p]);
		srb_advect_dye[p]->SetResource("dout", dye_ab[1 - p]);
		srb_advect_dye[p]->FlushDescriptorWrites();
	}

	// separable blur: horizontal reads the freshly advected dye[1-p]
	for (Int p = 0; p < 2; ++p)
	{
		pso_blur->CreateShaderResourceBinding(srb_blur_h[p], false);
		srb_blur_h[p]->SetResource("bp", bdir_h);
		srb_blur_h[p]->SetResource("src_buf", dye_ab[1 - p]);
		srb_blur_h[p]->SetResource("dst_buf", blur_tmp);
		srb_blur_h[p]->FlushDescriptorWrites();
	}

	pso_blur->CreateShaderResourceBinding(srb_blur_v, false);
	srb_blur_v->SetResource("bp", bdir_v);
	srb_blur_v->SetResource("src_buf", blur_tmp);
	srb_blur_v->SetResource("dst_buf", blur_out);
	srb_blur_v->FlushDescriptorWrites();

	for (Int p = 0; p < 2; ++p)
	{
		pso_display->CreateShaderResourceBinding(srb_display[p], false);
		srb_display[p]->SetResource("dye_buf", dye_ab[1 - p]);
		srb_display[p]->SetResource("blur_buf", blur_out);
		srb_display[p]->FlushDescriptorWrites();
	}
}

void RenderTest::RecordFrame(RHI::CommandList* in_cmd_list)
{
	Int p = frame_parity;

	// fp: [0]=dt [1]=mouse_x [2]=mouse_y [3]=force_x [4]=force_y
	//     [5]=is_down [6]=is_radial [7]=radius [8]=dye_amount
	Float32 params[12] = {};
	params[0] = cur_dt;
	params[1] = mouse_gx;
	params[2] = mouse_gy;
	params[3] = force_x;
	params[4] = force_y;
	params[5] = mouse_down ? 1.0f : 0.0f;
	params[6] = is_radial ? 1.0f : 0.0f;
	params[7] = SPLAT_RADIUS;
	params[8] = DYE_AMOUNT;
	UploadFloats(fp_buf, params, 12);

	auto run_compute = [&](RenderPipelineState* pso, ShaderResourceBinding* srb)
	{
		in_cmd_list->SetComputePipeline(pso);
		in_cmd_list->SetShaderResourceBinding(srb);
		in_cmd_list->Dispatch(GROUP_COUNT_X, GROUP_COUNT_Y, 1);
		// write->read for the next consumer, write->write for ping-pong safety;
		// both accumulate into one pipeline barrier flushed by the next Dispatch
		// or by SetRenderTarget (before BeginRendering)
		in_cmd_list->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::ShaderResource);
		in_cmd_list->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::UnorderedAccess);
	};

	run_compute(pso_splat, srb_splat[p]);
	run_compute(pso_advect_vel, srb_advect_vel);
	run_compute(pso_div, srb_div);
	for (Int i = 0; i < JACOBI_ITERS; ++i)
	{
		run_compute(pso_jacobi, srb_jacobi[i & 1]);
	}
	run_compute(pso_project, srb_project);
	run_compute(pso_advect_dye, srb_advect_dye[p]);
	run_compute(pso_blur, srb_blur_h[p]);
	run_compute(pso_blur, srb_blur_v);

	// fullscreen styled display (same skeleton as the Texture sample)
	Vector<ClearValue> clear_values;
	Vector<Texture*> rtvs = { m_viewport->GetCurrentBackBufferRTV() };
	Texture* dsv = m_viewport->GetCurrentBackBufferDSV();
	for (auto rtv : rtvs)
		clear_values.push_back(rtv->GetTextureDesc().clear_value);
	if (dsv)
		clear_values.push_back(dsv->GetTextureDesc().clear_value);
	in_cmd_list->SetRenderTarget(rtvs, dsv, clear_values, dsv != nullptr);
	in_cmd_list->SetGraphicsPipeline(pso_display);
	in_cmd_list->SetShaderResourceBinding(srb_display[p]);
	DrawAttribute draw_attr;
	draw_attr.vertexCount = 3;
	draw_attr.instanceCount = 1;
	in_cmd_list->Draw(draw_attr);

	frame_parity ^= 1;
}

void RenderTest::OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport)
{
	m_window = in_window;
	m_viewport = in_viewport;
	std::cout << "Hello Fluid2D" << std::endl;

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

	// Step 2: Create simulation resources
	CreateFluidBuffers();
	CreateFluidPipelines();
	CreateFluidBindings();

	// Step 3: Single pass: sim dispatches then fullscreen styled draw
	auto* rdg_pass = graph.AddRenderPass<FluidData>("Fluid2DPass", &graph, cmd_list,
	[&](FluidData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd_list)
	{
		builder.Write(rt_resource);
		if (ds_resource) builder.Write(ds_resource);
	},
	[=](CONST FluidData& data, CommandList* in_cmd_list)
	{
		this->RecordFrame(in_cmd_list);
	});

	rdg_pass->SetIsCullable(false);
	graph.Compile();
}

void RenderTest::OnShutdown_Logic()
{
	graph.Release();

	RHI::ShaderResourceBinding* srbs[] = {
		srb_splat[0], srb_splat[1], srb_advect_vel, srb_div,
		srb_jacobi[0], srb_jacobi[1], srb_project,
		srb_advect_dye[0], srb_advect_dye[1],
		srb_blur_h[0], srb_blur_h[1], srb_blur_v,
		srb_display[0], srb_display[1] };
	for (auto* srb : srbs)
	{
		delete srb;
	}

	RHI::Buffer* buffers[] = {
		vel_a, vel_b, p_a, p_b, div_buf, dye_ab[0], dye_ab[1],
		blur_tmp, blur_out, fp_buf, bdir_h, bdir_v };
	for (auto* buf : buffers)
	{
		delete buf;
	}
}

void RenderTest::OnUpdate(float dt)
{
	PlatformWindow* pw = GetPlatformWindow();
	int win_w = 0, win_h = 0;
	pw->GetFramebufferSize(win_w, win_h);
	Float64 cx = 0.0, cy = 0.0;
	pw->GetCursorPos(cx, cy);
	mouse_down = pw->GetMouseButton(MXRender::MouseButton::Left);

	Float64 dx = has_last_cursor ? cx - last_cursor_x : 0.0;
	Float64 dy = has_last_cursor ? cy - last_cursor_y : 0.0;
	last_cursor_x = cx;
	last_cursor_y = cy;
	has_last_cursor = true;

	if (win_w <= 0 || win_h <= 0)
	{
		mouse_down = false;
		return;
	}

	// cursor pixel coords -> grid cell coords (both are top-left origin, y down)
	mouse_gx = (Float32)(cx * (Float64)FLUID_W / (Float64)win_w);
	mouse_gy = (Float32)(cy * (Float64)FLUID_H / (Float64)win_h);
	is_radial = mouse_down && (dx * dx + dy * dy < 4.0);
	force_x = (Float32)(dx * (Float64)FLUID_W / (Float64)win_w) * FORCE_SCALE;
	force_y = (Float32)(dy * (Float64)FLUID_H / (Float64)win_h) * FORCE_SCALE;
	cur_dt = dt < MAX_DT ? dt : MAX_DT;
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
