#include "Input/InputSystem.h"
// VolumetricCloud: Hillaire-style sky LUTs + simplified raymarching (Unity ref).
// Pipeline: March -> Filter -> TAA(EMA blend) -> Copy(hist) -> Composite
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <fstream>
#include <cstring>
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

static CONST UInt32 SHAPE_N = 128, DETAIL_N = 32, WEATHER_N = 512;
static CONST UInt32 TRANS_W = 256, TRANS_H = 64, SKY_W = 192, SKY_H = 108;
static CONST UInt32 HALF_W = 640, HALF_H = 480;
static CONST Float32 CLOUD_BOTTOM = 1500.0f, CLOUD_TOP = 4000.0f;
static CONST Float32 FOG_DENSITY = 0.00006f;

static Float32 g_scroll = 0.0f;
static void ScrollCB(Float64, Float64 y) { g_scroll += (Float32)y; }

Vector<UInt32> RS(CONST String& fn) {
	std::ifstream f(fn, std::ios::ate | std::ios::binary);
	CHECK_WITH_LOG(!f.is_open(), "fail"); size_t sz = (size_t)f.tellg();
	Vector<UInt32> b(sz / sizeof(UInt32)); f.seekg(0); f.read((char*)b.data(), sz);
	return std::move(b);
}
static Shader* LF(ENUM_SHADER_STAGE st, CONST String& fn) {
	ShaderDesc d; d.shader_type = st; d.entry_name = "main"; d.shader_name = fn;
	ShaderDataPayload p; p.data = RS(fn); return g_render_rhi->CreateShader(d, p);
}
static RenderPipelineState* MkPSO(Shader* cs) {
	RenderGraphiPipelineStateDesc d{};
	d.shaders[ENUM_SHADER_STAGE::Shader_Compute] = cs;
	d.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
	d.raster_state.sample_count = 1;
	return g_render_rhi->CreateRenderPipelineState(d);
}
static void UF(Buffer* b, CONST Float32* d, UInt32 n) {
	void* p = RHIMapBuffer(b, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::None);
	std::memcpy(p, d, n * sizeof(Float32)); RHIUnmapBuffer(b);
}

struct PD : public RenderGraphPassDataBase {
	RenderPipelineState* pso = nullptr; ShaderResourceBinding* srb = nullptr;
	VIRTUAL ~PD() MYDEFAULT;
	VIRTUAL void Release() OVERRIDE { delete srb; }
};

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RT, public MXRender::RenderInterface)
public:
	RT(PlatformWindow* w) : m_window(w) {} RT() MYDEFAULT; VIRTUAL ~RT() MYDEFAULT;
	VIRTUAL void OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport) OVERRIDE FINAL;
	VIRTUAL void OnShutdown_Logic() OVERRIDE FINAL;
	VIRTUAL void OnUpdate(float) OVERRIDE FINAL;
	VIRTUAL void OnRender() OVERRIDE FINAL;
	PlatformWindow* GetPlatformWindow() { return m_window; }
protected:
	void CreateRes();
	void CreateCSO();
	void CreateSRBs();
	void RecNoise(RHI::CommandList*);
	void RecAtmo(RHI::CommandList*);
	void RecFilter(RHI::CommandList*);
	void RecTAA(RHI::CommandList*);
	void RecCopy(RHI::CommandList*);
	void RecMarch(RHI::CommandList*);
	PlatformWindow* m_window = nullptr;
	RHI::Viewport* m_viewport = nullptr;
	RHI::Buffer* op_buf = nullptr;
	RHI::Texture *tex_shape=nullptr,*tex_detail=nullptr,*tex_weather=nullptr,
	             *tex_trans=nullptr,*tex_skyview=nullptr,*tex_cloud=nullptr,
	             *tex_cloud_filt=nullptr,*tex_cloud_hist=nullptr,*tex_cloud_taa=nullptr;
	RHI::RenderPipelineState *pShape=nullptr,*pDetail=nullptr,*pWeather=nullptr,
	                         *pTrans=nullptr,*pSky=nullptr,*pMarch=nullptr,
	                         *pFilter=nullptr,*pTAA=nullptr,*pCopy=nullptr;
	RHI::ShaderResourceBinding *sShape=nullptr,*sDetail=nullptr,*sWeather=nullptr,
	                           *sTrans=nullptr,*sSky=nullptr,*sMarch=nullptr,
	                           *sFilter=nullptr,*sTAA=nullptr,*sCopy=nullptr;
	UInt32 fi = 0; Float32 ts = 0;
	Float32 cam_yaw=0.2f, cam_pitch=0.12f, cam_h=5.0f; Bool drag=false; Float64 lcx=0,lcy=0;
	glm::vec3 eye = glm::vec3(0,5,0);
	glm::mat4 vp = glm::mat4(1), ivp = glm::mat4(1);
	Float32 sun_azim=0.35f, sun_elev=0.55f;
	Float32 coverage=0.55f, density=0.15f, wind_speed=12.0f, exposure=1.2f;
	glm::vec2 wind_off = glm::vec2(0);
	glm::vec3 sun = glm::vec3(0,1,0);
	Int dm = 0; Bool kp[5] = {};
MYRENDERER_END_CLASS

void RT::CreateRes() {
	BufferDesc od; od.type = ENUM_BUFFER_TYPE::Storage | ENUM_BUFFER_TYPE::Dynamic;
	od.size = od.stride = 64 * sizeof(Float32); op_buf = g_render_rhi->CreateBuffer(od);
	Float32 z[64] = {}; UF(op_buf, z, 64);

	auto mk2d = [](UInt32 w, UInt32 h) {
		RHI::TextureDesc td{}; td.width = w; td.height = h;
		td.format = ENUM_TEXTURE_FORMAT::RGBA16F; td.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
		td.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_STORAGE | ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
		return g_render_rhi->CreateTexture(td);
	};
	auto mk3d = [](UInt32 n) {
		RHI::TextureDesc td{}; td.width = n; td.height = n; td.depth = (UInt16)n;
		td.format = ENUM_TEXTURE_FORMAT::RGBA16F; td.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_3D;
		td.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_STORAGE | ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
		return g_render_rhi->CreateTexture(td);
	};
	tex_shape = mk3d(SHAPE_N); tex_detail = mk3d(DETAIL_N);
	tex_weather = mk2d(WEATHER_N, WEATHER_N);
	tex_trans = mk2d(TRANS_W, TRANS_H); tex_skyview = mk2d(SKY_W, SKY_H);
	tex_cloud = mk2d(HALF_W, HALF_H); tex_cloud_filt = mk2d(HALF_W, HALF_H);
	tex_cloud_hist = mk2d(HALF_W, HALF_H); tex_cloud_taa = mk2d(HALF_W, HALF_H);
}
void RT::CreateCSO() {
	Shader* cs[9] = {
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/cloud_noise_shape.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/cloud_noise_detail.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/cloud_noise_weather.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/cloud_atmo_transmittance.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/cloud_atmo_skyview.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/cloud_march.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/cloud_filter.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/cloud_taa.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/cloud_copy.comp.spv") };
	pShape=MkPSO(cs[0]); pDetail=MkPSO(cs[1]); pWeather=MkPSO(cs[2]);
	pTrans=MkPSO(cs[3]); pSky=MkPSO(cs[4]); pMarch=MkPSO(cs[5]);
	pFilter=MkPSO(cs[6]); pTAA=MkPSO(cs[7]); pCopy=MkPSO(cs[8]);
	for (auto* s : cs) delete s;
}
void RT::CreateSRBs() {
	pShape->CreateShaderResourceBinding(sShape, false);
	sShape->SetResource("shape_img", tex_shape); sShape->FlushDescriptorWrites();
	pDetail->CreateShaderResourceBinding(sDetail, false);
	sDetail->SetResource("detail_img", tex_detail); sDetail->FlushDescriptorWrites();
	pWeather->CreateShaderResourceBinding(sWeather, false);
	sWeather->SetResource("weather_img", tex_weather); sWeather->FlushDescriptorWrites();
	pTrans->CreateShaderResourceBinding(sTrans, false);
	sTrans->SetResource("trans_img", tex_trans); sTrans->FlushDescriptorWrites();
	pSky->CreateShaderResourceBinding(sSky, false);
	sSky->SetResource("op", op_buf); sSky->SetResource("trans_lut", tex_trans);
	sSky->SetResource("skyview_img", tex_skyview); sSky->FlushDescriptorWrites();
	pMarch->CreateShaderResourceBinding(sMarch, false);
	sMarch->SetResource("op", op_buf);
	sMarch->SetResource("shape_tex", tex_shape); sMarch->SetResource("detail_tex", tex_detail);
	sMarch->SetResource("weather_tex", tex_weather); sMarch->SetResource("trans_lut", tex_trans);
	sMarch->SetResource("skyview_tex", tex_skyview); sMarch->SetResource("cloud_img", tex_cloud);
	sMarch->FlushDescriptorWrites();
	pFilter->CreateShaderResourceBinding(sFilter, false);
	sFilter->SetResource("op", op_buf);
	sFilter->SetResource("cloud_in", tex_cloud);
	sFilter->SetResource("cloud_out", tex_cloud_filt);
	sFilter->FlushDescriptorWrites();
	pTAA->CreateShaderResourceBinding(sTAA, false);
	sTAA->SetResource("op", op_buf);
	sTAA->SetResource("cloud_curr", tex_cloud_filt);
	sTAA->SetResource("cloud_hist", tex_cloud_hist);
	sTAA->SetResource("cloud_taa", tex_cloud_taa);
	sTAA->FlushDescriptorWrites();
	pCopy->CreateShaderResourceBinding(sCopy, false);
	sCopy->SetResource("op", op_buf);
	sCopy->SetResource("src_tex", tex_cloud_taa);
	sCopy->SetResource("dst_img", tex_cloud_hist);
	sCopy->FlushDescriptorWrites();
}

void RT::RecNoise(RHI::CommandList* c) {
	if (fi != 0) return;
	auto rc = [c](RenderPipelineState* pso, ShaderResourceBinding* s, UInt32 x, UInt32 y, UInt32 z) {
		c->SetComputePipeline(pso); c->SetShaderResourceBinding(s); c->Dispatch(x, y, z);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::ShaderResource);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::UnorderedAccess);
	};
	c->TransitionTextureState(tex_shape, ENUM_RESOURCE_STATE::UnorderedAccess);
	c->TransitionTextureState(tex_detail, ENUM_RESOURCE_STATE::UnorderedAccess);
	c->TransitionTextureState(tex_weather, ENUM_RESOURCE_STATE::UnorderedAccess);
	rc(pShape, sShape, SHAPE_N / 8, SHAPE_N / 8, SHAPE_N / 8);
	rc(pDetail, sDetail, DETAIL_N / 8, DETAIL_N / 8, DETAIL_N / 8);
	rc(pWeather, sWeather, WEATHER_N / 16, WEATHER_N / 16, 1);
	c->TransitionTextureState(tex_shape, ENUM_RESOURCE_STATE::ShaderResource);
	c->TransitionTextureState(tex_detail, ENUM_RESOURCE_STATE::ShaderResource);
	c->TransitionTextureState(tex_weather, ENUM_RESOURCE_STATE::ShaderResource);
}
void RT::RecAtmo(RHI::CommandList* c) {
	Float32 p[64] = {};
	std::memcpy(p, &vp, 64); std::memcpy(p + 16, &ivp, 64);
	p[32]=eye.x; p[33]=eye.y; p[34]=eye.z; p[35]=ts;
	p[36]=sun.x; p[37]=sun.y; p[38]=sun.z; p[39]=(Float32)dm;
	p[40]=wind_off.x; p[41]=wind_off.y; p[42]=coverage; p[43]=density;
	p[44]=CLOUD_BOTTOM; p[45]=CLOUD_TOP; p[46]=(Float32)HALF_W; p[47]=(Float32)HALF_H;
	p[48]=wind_speed; p[49]=exposure; p[50]=FOG_DENSITY; p[51]=ts*0.05f;
	p[52]=(Float32)(fi % 8);
	UF(op_buf, p, 64);
	auto rc = [c](RenderPipelineState* pso, ShaderResourceBinding* s, UInt32 x, UInt32 y) {
		c->SetComputePipeline(pso); c->SetShaderResourceBinding(s); c->Dispatch(x, y, 1);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::ShaderResource);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::UnorderedAccess);
	};
	if (fi == 0) {
		c->TransitionTextureState(tex_trans, ENUM_RESOURCE_STATE::UnorderedAccess);
		rc(pTrans, sTrans, TRANS_W / 8, TRANS_H / 8);
		c->TransitionTextureState(tex_trans, ENUM_RESOURCE_STATE::ShaderResource);
	}
	c->TransitionTextureState(tex_skyview, ENUM_RESOURCE_STATE::UnorderedAccess);
	rc(pSky, sSky, SKY_W / 8, (SKY_H + 7) / 8);
	c->TransitionTextureState(tex_skyview, ENUM_RESOURCE_STATE::ShaderResource);
	fi++;
}
void RT::RecFilter(RHI::CommandList* c) {
	c->TransitionTextureState(tex_cloud_filt, ENUM_RESOURCE_STATE::UnorderedAccess);
	c->SetComputePipeline(pFilter); c->SetShaderResourceBinding(sFilter);
	c->Dispatch(HALF_W / 8, HALF_H / 8, 1);
	c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::ShaderResource);
	c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::UnorderedAccess);
	c->TransitionTextureState(tex_cloud_filt, ENUM_RESOURCE_STATE::ShaderResource);
}
void RT::RecTAA(RHI::CommandList* c) {
	c->TransitionTextureState(tex_cloud_taa, ENUM_RESOURCE_STATE::UnorderedAccess);
	c->SetComputePipeline(pTAA); c->SetShaderResourceBinding(sTAA);
	c->Dispatch(HALF_W / 8, HALF_H / 8, 1);
	c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::ShaderResource);
	c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::UnorderedAccess);
	c->TransitionTextureState(tex_cloud_taa, ENUM_RESOURCE_STATE::ShaderResource);
}
void RT::RecCopy(RHI::CommandList* c) {
	c->TransitionTextureState(tex_cloud_hist, ENUM_RESOURCE_STATE::UnorderedAccess);
	c->SetComputePipeline(pCopy); c->SetShaderResourceBinding(sCopy);
	c->Dispatch(HALF_W / 8, HALF_H / 8, 1);
	c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::ShaderResource);
	c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::UnorderedAccess);
	c->TransitionTextureState(tex_cloud_hist, ENUM_RESOURCE_STATE::ShaderResource);
}
void RT::RecMarch(RHI::CommandList* c) {
	c->TransitionTextureState(tex_cloud, ENUM_RESOURCE_STATE::UnorderedAccess);
	c->SetComputePipeline(pMarch); c->SetShaderResourceBinding(sMarch);
	c->Dispatch(HALF_W / 8, HALF_H / 8, 1);
	c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::ShaderResource);
	c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess, ENUM_RESOURCE_STATE::UnorderedAccess);
	c->TransitionTextureState(tex_cloud, ENUM_RESOURCE_STATE::ShaderResource);
}

void RT::OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport) {
	m_window = in_window; m_viewport = in_viewport;
	std::cout << "Hello VolumetricCloud" << std::endl;
	m_window->SetScrollCallback(ScrollCB);
	RHI::CommandList* cl = RHIGetImmediateCommandList();
	RHI::Texture* bb = m_viewport->GetCurrentBackBufferRTV();
	RHI::Texture* ds = m_viewport->GetCurrentBackBufferDSV();
	CreateRes(); CreateCSO(); CreateSRBs();

	auto* rt = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("RT", bb->GetTextureDesc(), bb);
	RenderGraphResource<RHI::TextureDesc, RHI::Texture>* dv = nullptr;
	if (ds) dv = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("DS", ds->GetTextureDesc(), ds);
	auto* shp = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("Shape", tex_shape->GetTextureDesc(), tex_shape);
	auto* det = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("Detail", tex_detail->GetTextureDesc(), tex_detail);
	auto* wea = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("Weather", tex_weather->GetTextureDesc(), tex_weather);
	auto* trn = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("TransLUT", tex_trans->GetTextureDesc(), tex_trans);
	auto* sky = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("SkyViewLUT", tex_skyview->GetTextureDesc(), tex_skyview);
	auto* cld = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("CloudBuf", tex_cloud->GetTextureDesc(), tex_cloud);
	auto* flt = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("CloudFilt", tex_cloud_filt->GetTextureDesc(), tex_cloud_filt);
	auto* hst = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("CloudHist", tex_cloud_hist->GetTextureDesc(), tex_cloud_hist);
	auto* taa = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("CloudTAA", tex_cloud_taa->GetTextureDesc(), tex_cloud_taa);

	auto ap = [&](const char* name, auto setup, auto exec) {
		auto* p = graph.AddRenderPass<PD>(name, &graph, cl, setup, exec); p->SetIsCullable(false); return p;
	};
	ap("CloudNoiseGen",
		[&](PD&, RenderGraphPassBuilder& b, CommandList*) {
			b.Write(shp, ENUM_RESOURCE_STATE::UnorderedAccess);
			b.Write(det, ENUM_RESOURCE_STATE::UnorderedAccess);
			b.Write(wea, ENUM_RESOURCE_STATE::UnorderedAccess);
		},
		[=](CONST PD&, CommandList* c) { RecNoise(c); });
	ap("CloudAtmoLUT",
		[&](PD&, RenderGraphPassBuilder& b, CommandList*) {
			b.Write(trn, ENUM_RESOURCE_STATE::UnorderedAccess);
			b.Write(sky, ENUM_RESOURCE_STATE::UnorderedAccess);
		},
		[=](CONST PD&, CommandList* c) { RecAtmo(c); });
	ap("CloudMarch",
		[&](PD&, RenderGraphPassBuilder& b, CommandList*) {
			b.Read(shp, ENUM_RESOURCE_STATE::ShaderResource);
			b.Read(det, ENUM_RESOURCE_STATE::ShaderResource);
			b.Read(wea, ENUM_RESOURCE_STATE::ShaderResource);
			b.Read(sky, ENUM_RESOURCE_STATE::ShaderResource);
			b.Write(cld, ENUM_RESOURCE_STATE::UnorderedAccess);
		},
		[=](CONST PD&, CommandList* c) { RecMarch(c); });
	ap("CloudFilter",
		[&](PD&, RenderGraphPassBuilder& b, CommandList*) {
			b.Read(cld, ENUM_RESOURCE_STATE::ShaderResource);
			b.Write(flt, ENUM_RESOURCE_STATE::UnorderedAccess);
		},
		[=](CONST PD&, CommandList* c) { RecFilter(c); });
	ap("CloudTAA",
		[&](PD&, RenderGraphPassBuilder& b, CommandList*) {
			b.Read(flt, ENUM_RESOURCE_STATE::ShaderResource);
			b.Read(hst, ENUM_RESOURCE_STATE::ShaderResource);
			b.Write(taa, ENUM_RESOURCE_STATE::UnorderedAccess);
		},
		[=](CONST PD&, CommandList* c) { RecTAA(c); });
	ap("CloudCopy",
		[&](PD&, RenderGraphPassBuilder& b, CommandList*) {
			b.Read(taa, ENUM_RESOURCE_STATE::ShaderResource);
			b.Write(hst, ENUM_RESOURCE_STATE::UnorderedAccess);
		},
		[=](CONST PD&, CommandList* c) { RecCopy(c); });

	auto* p4 = graph.AddRenderPass<PD>("CloudComposite", &graph, cl,
	[&](PD& d, RenderGraphPassBuilder& b, CommandList*) {
		b.Read(trn, ENUM_RESOURCE_STATE::ShaderResource);
		b.Read(sky, ENUM_RESOURCE_STATE::ShaderResource);
		b.Read(taa, ENUM_RESOURCE_STATE::ShaderResource);
		b.Write(rt); if (dv) b.Write(dv);
		Shader* vs = LF(ENUM_SHADER_STAGE::Shader_Vertex, "Shader/cloud_fullscreen.vert.spv");
		Shader* ps = LF(ENUM_SHADER_STAGE::Shader_Pixel, "Shader/cloud_composite.frag.spv");
		RenderGraphiPipelineStateDesc pd{}; pd.shaders[0] = vs; pd.shaders[1] = ps;
		pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		Vector<Texture*> rts = { bb }; pd.render_targets = rts; pd.depth_stencil_view = ds;
		pd.raster_state.sample_count = 1; pd.raster_state.cull_mode = ENUM_RASTER_CULLMODE::None;
		pd.blend_state.render_targets.resize(1);
		d.pso = g_render_rhi->CreateRenderPipelineState(pd);
		d.pso->CreateShaderResourceBinding(d.srb, false);
		d.srb->SetResource("op", op_buf);
		d.srb->SetResource("trans_lut", tex_trans);
		d.srb->SetResource("skyview_tex", tex_skyview);
		d.srb->SetResource("cloud_tex", tex_cloud_taa);
		d.srb->SetResource("shape_tex", tex_shape);
		d.srb->SetResource("weather_tex", tex_weather);
		d.srb->FlushDescriptorWrites();
		delete vs; delete ps;
	},
	[=](CONST PD& d, CommandList* c) {
		Vector<ClearValue> cc; cc.push_back({ 0.02f,0.03f,0.05f,1.0f });
		Texture* d2 = m_viewport->GetCurrentBackBufferDSV();
		if (d2) cc.push_back({ 1.0f,0 });
		c->SetRenderTarget({ m_viewport->GetCurrentBackBufferRTV() }, d2, cc, d2 != nullptr);
		c->SetGraphicsPipeline(d.pso); c->SetShaderResourceBinding(d.srb);
		c->Draw(DrawAttribute{ 3,1,0,0 });
	}); p4->SetIsCullable(false);

	graph.Compile();
}
void RT::OnShutdown_Logic() {
	graph.Release();
	for (auto* s : { sShape,sDetail,sWeather,sTrans,sSky,sMarch,sFilter,sTAA,sCopy }) delete s;
	for (auto* t : { tex_shape,tex_detail,tex_weather,tex_trans,tex_skyview,tex_cloud,tex_cloud_filt,tex_cloud_hist,tex_cloud_taa }) delete t;
	delete op_buf;
}
void RT::OnUpdate(float dt) {
	auto& input = MXRender::Input::InputSystem::Get();
	Int ww, wh; m_window->GetFramebufferSize(ww, wh);
	Float32 cx, cy2; input.GetMousePos(cx, cy2);
	Bool dn = input.IsMouseDown(0); // Left
	if (dn && drag) {
		cam_yaw += (cx - (Float32)lcx) * 0.003f;
		cam_pitch = glm::clamp(cam_pitch + ((Float32)lcy - cy2) * 0.003f, -0.45f, 1.5f);
	}
	drag = dn; lcx = cx; lcy = cy2;
	cam_h = glm::clamp(cam_h * powf(1.25f, g_scroll), 2.0f, 6000.0f); g_scroll = 0;

	// Keyboard debug toggles via InputSystem
	Bool kn[5]; kn[0]=input.IsKeyDown(MXRender::Input::EKey::K0);
	kn[1]=input.IsKeyDown(MXRender::Input::EKey::K1); kn[2]=input.IsKeyDown(MXRender::Input::EKey::K2);
	kn[3]=input.IsKeyDown(MXRender::Input::EKey::K3); kn[4]=input.IsKeyDown(MXRender::Input::EKey::K4);
	for (Int i = 0; i < 5; ++i) { if (kn[i] && !kp[i]) dm = i; kp[i] = kn[i]; }
	if (input.IsKeyDown(MXRender::Input::EKey::Left) )  sun_azim -= 0.6f * dt;
	if (input.IsKeyDown(MXRender::Input::EKey::Right) ) sun_azim += 0.6f * dt;
	if (input.IsKeyDown(MXRender::Input::EKey::Up) )    sun_elev += 0.4f * dt;
	if (input.IsKeyDown(MXRender::Input::EKey::Down) )  sun_elev -= 0.4f * dt;
	sun_elev = glm::clamp(sun_elev, -0.10f, 1.50f);
	if (input.IsKeyDown(MXRender::Input::EKey::Q) ) coverage += 0.25f * dt;
	if (input.IsKeyDown(MXRender::Input::EKey::A) ) coverage -= 0.25f * dt;
	coverage = glm::clamp(coverage, 0.0f, 1.0f);
	if (input.IsKeyDown(MXRender::Input::EKey::W) ) density += 0.5f * dt;
	if (input.IsKeyDown(MXRender::Input::EKey::S) ) density -= 0.5f * dt;
	density = glm::clamp(density, 0.1f, 4.0f);
	if (input.IsKeyDown(MXRender::Input::EKey::E) ) wind_speed += 20.0f * dt;
	if (input.IsKeyDown(MXRender::Input::EKey::D) ) wind_speed -= 20.0f * dt;
	wind_speed = glm::clamp(wind_speed, 0.0f, 100.0f);

	if (ww <= 0 || wh <= 0) return;
	sun = glm::normalize(glm::vec3(cosf(sun_elev) * sinf(sun_azim), sinf(sun_elev), cosf(sun_elev) * cosf(sun_azim)));
	wind_off += glm::vec2(0.8f, 0.6f) * wind_speed * dt;
	eye = glm::vec3(0, cam_h, 0);
	glm::vec3 fwd(cosf(cam_pitch) * sinf(cam_yaw), sinf(cam_pitch), cosf(cam_pitch) * cosf(cam_yaw));
	glm::mat4 view = glm::lookAt(eye, eye + fwd, glm::vec3(0, 1, 0));
	glm::mat4 proj = glm::perspective(glm::radians(60.f), (Float32)ww / wh, 0.5f, 30000.f);
	proj[1][1] *= -1; vp = proj * view; ivp = glm::inverse(vp); ts += dt;
}
void RT::OnRender() { graph.Execute(); }
int main() { Window w; RT r(w.GetPlatformWindow()); w.InitWindow(); w.Run(&r); system("pause"); return 0; }
