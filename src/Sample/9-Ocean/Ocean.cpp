#include "Input/InputSystem.h"
// Ocean: FFT spectral waves via compute chain → storage buffers → graphics pass.
// Avoids sampler2D in graphics PSO by using storage buffers for displacement & normals.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <fstream>
#include <cstring>
#include <limits>
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

static CONST UInt32 FFT_N = 256, GRID_DIM = 512;
static CONST UInt32 OCEAN_VERTS = GRID_DIM * GRID_DIM * 6;
static CONST UInt32 BUF_ELMS = FFT_N * FFT_N * 4; // 256*256*4 = 262144 floats
static CONST Float32 PATCH_L = 51.2f, MESH_SIZE = 819.2f;
static CONST Float32 CHOPPY = 1.1f, WIND_SPEED = 8.0f, AMPL = 1.5e-5f, FOG_DENSITY = 0.0045f;

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
	void RecSpec(RHI::CommandList*);
	void RecFFT(RHI::CommandList*);
	void RecUnpack(RHI::CommandList*);
	void RecExport(RHI::CommandList*);
	PlatformWindow* m_window = nullptr;
	RHI::Viewport* m_viewport = nullptr;
	RHI::Buffer *op_buf=nullptr, *disp_buf=nullptr, *norm_buf=nullptr;
	RHI::Texture *h0=nullptr,*spec=nullptr,*tmp=nullptr,*spat=nullptr,*dmap=nullptr,*nmap=nullptr;
	RHI::RenderPipelineState *pInit=nullptr,*pSpec=nullptr,*pR=nullptr,*pC=nullptr,*pUnp=nullptr,*pExp=nullptr;
	RHI::ShaderResourceBinding *sInit=nullptr,*sSpec=nullptr,*sR=nullptr,*sC=nullptr,*sUnp=nullptr,*sExp=nullptr;
	UInt32 fi=0; Float32 ts=0, ampl=AMPL; Int dm=0; Bool ds=false, kp[7]={};
	Float32 cy=0.7f,cp=0.25f,cd=60; Bool drag=false; Float64 lcx=0,lcy=0;
	glm::vec3 eye=glm::vec3(0,25,50),sun=glm::normalize(glm::vec3(-0.55,0.72,-0.60));
	glm::mat4 vp=glm::mat4(1), ivp=glm::mat4(1);
MYRENDERER_END_CLASS

void RT::CreateRes() {
	BufferDesc od; od.type=ENUM_BUFFER_TYPE::Storage|ENUM_BUFFER_TYPE::Dynamic;
	od.size=od.stride=64*sizeof(Float32); op_buf=g_render_rhi->CreateBuffer(od);
	Float32 z[64]={}; UF(op_buf,z,64);

	RHI::TextureDesc fd{}; fd.width=FFT_N; fd.height=FFT_N;
	fd.format=ENUM_TEXTURE_FORMAT::RGBA32F; fd.type=ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	fd.usage=ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_STORAGE;
	h0=g_render_rhi->CreateTexture(fd); spec=g_render_rhi->CreateTexture(fd); tmp=g_render_rhi->CreateTexture(fd); spat=g_render_rhi->CreateTexture(fd);

	RHI::TextureDesc md{}; md.width=FFT_N; md.height=FFT_N;
	md.format=ENUM_TEXTURE_FORMAT::RGBA16F; md.type=ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	md.usage=ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_STORAGE|ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
	dmap=g_render_rhi->CreateTexture(md); nmap=g_render_rhi->CreateTexture(md);

	BufferDesc bd; bd.type=ENUM_BUFFER_TYPE::Storage|ENUM_BUFFER_TYPE::Dynamic;
	bd.size=bd.stride=BUF_ELMS*sizeof(Float32);
	disp_buf=g_render_rhi->CreateBuffer(bd); norm_buf=g_render_rhi->CreateBuffer(bd);
}
void RT::CreateCSO() {
	Shader* cs[6]={
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/ocean_init_spectrum.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/ocean_spectrum.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/ocean_fft_rows.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/ocean_fft_cols.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/ocean_unpack.comp.spv"),
		LF(ENUM_SHADER_STAGE::Shader_Compute,"Shader/ocean_buffer_export.comp.spv")};
	pInit=MkPSO(cs[0]); pSpec=MkPSO(cs[1]); pR=MkPSO(cs[2]); pC=MkPSO(cs[3]); pUnp=MkPSO(cs[4]); pExp=MkPSO(cs[5]);
	for(auto* s:cs) delete s;
}
void RT::CreateSRBs() {
	pInit->CreateShaderResourceBinding(sInit,false);
	sInit->SetResource("op",op_buf); sInit->SetResource("h0_img",h0); sInit->FlushDescriptorWrites();
	pSpec->CreateShaderResourceBinding(sSpec,false);
	sSpec->SetResource("op",op_buf); sSpec->SetResource("h0_img",h0);
	sSpec->SetResource("spec_img",spec); sSpec->FlushDescriptorWrites();
	pR->CreateShaderResourceBinding(sR,false);
	sR->SetResource("src_img",spec); sR->SetResource("dst_img",tmp); sR->FlushDescriptorWrites();
	pC->CreateShaderResourceBinding(sC,false);
	sC->SetResource("src_img",tmp); sC->SetResource("dst_img",spat); sC->FlushDescriptorWrites();
	pUnp->CreateShaderResourceBinding(sUnp,false);
	sUnp->SetResource("op",op_buf); sUnp->SetResource("spatial_img",spat);
	sUnp->SetResource("disp_img",dmap); sUnp->SetResource("normal_img",nmap); sUnp->FlushDescriptorWrites();
	pExp->CreateShaderResourceBinding(sExp,false);
	sExp->SetResource("disp_img",dmap); sExp->SetResource("norm_img",nmap);
	sExp->SetResource("disp_buf",disp_buf); sExp->SetResource("norm_buf",norm_buf); sExp->FlushDescriptorWrites();
}

void RT::RecSpec(RHI::CommandList* c) {
	Float32 p[64]={}; p[0]=ts; p[1]=PATCH_L; p[2]=CHOPPY; p[3]=(Float32)FFT_N;
	p[4]=eye.x; p[5]=eye.y; p[6]=eye.z; p[7]=MESH_SIZE;
	p[8]=sun.x; p[9]=sun.y; p[10]=sun.z; p[11]=(Float32)GRID_DIM;
	p[12]=WIND_SPEED; p[13]=1.0f; p[14]=0.35f; p[15]=ampl;
	std::memcpy(p+16,&vp,64); std::memcpy(p+32,&ivp,64); p[48]=FOG_DENSITY; p[58]=(Float32)dm; p[59]=ds?1.f:0.f;
	UF(op_buf,p,64);
	auto rc=[c](RenderPipelineState* pso,ShaderResourceBinding* s,UInt32 x,UInt32 y){
		c->SetComputePipeline(pso); c->SetShaderResourceBinding(s); c->Dispatch(x,y,1);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess,ENUM_RESOURCE_STATE::ShaderResource);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess,ENUM_RESOURCE_STATE::UnorderedAccess);
	};
	UInt32 g=FFT_N/16;
	if(fi==0){c->TransitionTextureState(h0,ENUM_RESOURCE_STATE::UnorderedAccess);rc(pInit,sInit,g,g);}
	c->TransitionTextureState(spec,ENUM_RESOURCE_STATE::UnorderedAccess);rc(pSpec,sSpec,g,g);
	fi++;
}
void RT::RecFFT(RHI::CommandList* c) {
	auto rc=[c](RenderPipelineState* pso,ShaderResourceBinding* s,UInt32 x,UInt32 y){
		c->SetComputePipeline(pso); c->SetShaderResourceBinding(s); c->Dispatch(x,y,1);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess,ENUM_RESOURCE_STATE::ShaderResource);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess,ENUM_RESOURCE_STATE::UnorderedAccess);
	};
	c->TransitionTextureState(tmp,ENUM_RESOURCE_STATE::UnorderedAccess); rc(pR,sR,FFT_N,1);
	c->TransitionTextureState(spat,ENUM_RESOURCE_STATE::UnorderedAccess); rc(pC,sC,FFT_N,1);
}
void RT::RecUnpack(RHI::CommandList* c) {
	auto rc=[c](RenderPipelineState* pso,ShaderResourceBinding* s,UInt32 x,UInt32 y){
		c->SetComputePipeline(pso); c->SetShaderResourceBinding(s); c->Dispatch(x,y,1);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess,ENUM_RESOURCE_STATE::ShaderResource);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess,ENUM_RESOURCE_STATE::UnorderedAccess);
	};
	c->TransitionTextureState(dmap,ENUM_RESOURCE_STATE::UnorderedAccess);
	c->TransitionTextureState(nmap,ENUM_RESOURCE_STATE::UnorderedAccess);
	rc(pUnp,sUnp,FFT_N/16,FFT_N/16);
	c->TransitionTextureState(dmap,ENUM_RESOURCE_STATE::ShaderResource);
	c->TransitionTextureState(nmap,ENUM_RESOURCE_STATE::ShaderResource);
}
void RT::RecExport(RHI::CommandList* c) {
	auto rc=[c](RenderPipelineState* pso,ShaderResourceBinding* s,UInt32 x,UInt32 y){
		c->SetComputePipeline(pso); c->SetShaderResourceBinding(s); c->Dispatch(x,y,1);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess,ENUM_RESOURCE_STATE::ShaderResource);
		c->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess,ENUM_RESOURCE_STATE::UnorderedAccess);
	};
	rc(pExp,sExp,FFT_N/16,FFT_N/16);
}

void RT::OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport) {
	m_window = in_window; m_viewport = in_viewport;
	std::cout<<"Hello Ocean (FFT)"<<std::endl;
	m_window->SetScrollCallback(ScrollCB);
	RHI::CommandList* cl=RHIGetImmediateCommandList();
	RHI::Texture* bb=m_viewport->GetCurrentBackBufferRTV();
	RHI::Texture* ds=m_viewport->GetCurrentBackBufferDSV();
	CreateRes(); CreateCSO(); CreateSRBs();

	auto* rt=graph.AddRetainedResource<RHI::TextureDesc,RHI::Texture>("RT",bb->GetTextureDesc(),bb);
	RenderGraphResource<RHI::TextureDesc,RHI::Texture>* dv=nullptr;
	if(ds) dv=graph.AddRetainedResource<RHI::TextureDesc,RHI::Texture>("DS",ds->GetTextureDesc(),ds);
	auto* sr=graph.AddRetainedResource<RHI::TextureDesc,RHI::Texture>("Spec",spec->GetTextureDesc(),spec);
	auto* sp=graph.AddRetainedResource<RHI::TextureDesc,RHI::Texture>("Spat",spat->GetTextureDesc(),spat);
	auto* dr=graph.AddRetainedResource<RHI::TextureDesc,RHI::Texture>("Disp",dmap->GetTextureDesc(),dmap);
	auto* nr=graph.AddRetainedResource<RHI::TextureDesc,RHI::Texture>("Norm",nmap->GetTextureDesc(),nmap);

	auto ap=[&](const char* name, auto setup, auto exec){
		auto* p=graph.AddRenderPass<PD>(name,&graph,cl,setup,exec); p->SetIsCullable(false); return p;
	};
	ap("Spectrum", [&](PD&,RenderGraphPassBuilder& b,CommandList*){b.Write(sr,ENUM_RESOURCE_STATE::UnorderedAccess);},
		[=](CONST PD&,CommandList* c){RecSpec(c);});
	ap("FFT", [&](PD&,RenderGraphPassBuilder& b,CommandList*){b.Read(sr,ENUM_RESOURCE_STATE::UnorderedAccess);b.Write(sp,ENUM_RESOURCE_STATE::UnorderedAccess);},
		[=](CONST PD&,CommandList* c){RecFFT(c);});
	ap("Unpack", [&](PD&,RenderGraphPassBuilder& b,CommandList*){b.Read(sp,ENUM_RESOURCE_STATE::UnorderedAccess);b.Write(dr,ENUM_RESOURCE_STATE::UnorderedAccess);b.Write(nr,ENUM_RESOURCE_STATE::UnorderedAccess);},
		[=](CONST PD&,CommandList* c){RecUnpack(c);});
	ap("Export", [&](PD&,RenderGraphPassBuilder& b,CommandList*){b.Read(dr,ENUM_RESOURCE_STATE::ShaderResource);b.Read(nr,ENUM_RESOURCE_STATE::ShaderResource);},
		[=](CONST PD&,CommandList* c){RecExport(c);});

	// Sky pass
	auto* p5=graph.AddRenderPass<PD>("Sky",&graph,cl,
	[&](PD& d,RenderGraphPassBuilder& b,CommandList*){
		b.Write(rt); if(dv) b.Write(dv);
		Shader* vs=LF(ENUM_SHADER_STAGE::Shader_Vertex,"Shader/ocean_sky.vert.spv");
		Shader* ps=LF(ENUM_SHADER_STAGE::Shader_Pixel,"Shader/ocean_sky.frag.spv");
		RenderGraphiPipelineStateDesc pd{}; pd.shaders[0]=vs; pd.shaders[1]=ps;
		pd.primitive_topology=ENUM_PRIMITIVE_TYPE::TriangleList;
		Vector<Texture*> rts={bb}; pd.render_targets=rts; pd.depth_stencil_view=ds;
		pd.raster_state.sample_count=1; pd.raster_state.cull_mode=ENUM_RASTER_CULLMODE::None;
		pd.blend_state.render_targets.resize(1);
		d.pso=g_render_rhi->CreateRenderPipelineState(pd);
		d.pso->CreateShaderResourceBinding(d.srb,false);
		d.srb->SetResource("op",op_buf);
		d.srb->FlushDescriptorWrites();
		delete vs; delete ps;
	},
	[=](CONST PD& d,CommandList* c){
		Vector<ClearValue> cc; cc.push_back({0.2f,0.2f,0.3f,1.0f});
		Texture* d2=m_viewport->GetCurrentBackBufferDSV();
		if(d2) cc.push_back({1.0f,0});
		c->SetRenderTarget({m_viewport->GetCurrentBackBufferRTV()},d2,cc,d2!=nullptr);
		c->SetGraphicsPipeline(d.pso); c->SetShaderResourceBinding(d.srb);
		c->Draw(DrawAttribute{3,1,0,0});
	}); p5->SetIsCullable(false);

	// Ocean pass: storage buffers for displacement + normals
	auto* p6=graph.AddRenderPass<PD>("Ocean",&graph,cl,
	[&](PD& d,RenderGraphPassBuilder& b,CommandList*){
		b.Write(rt); if(dv) b.Write(dv);
		Shader* vs=LF(ENUM_SHADER_STAGE::Shader_Vertex,"Shader/ocean_surface.vert.spv");
		Shader* ps=LF(ENUM_SHADER_STAGE::Shader_Pixel,"Shader/ocean_surface.frag.spv");
		RenderGraphiPipelineStateDesc pd{}; pd.shaders[0]=vs; pd.shaders[1]=ps;
		pd.primitive_topology=ENUM_PRIMITIVE_TYPE::TriangleList;
		Vector<Texture*> rts={bb}; pd.render_targets=rts; pd.depth_stencil_view=ds;
		pd.raster_state.sample_count=1; pd.raster_state.cull_mode=ENUM_RASTER_CULLMODE::None;
		pd.depth_stencil_state.depth_test_enable=true;
		pd.depth_stencil_state.depth_write_enable=true;
		pd.depth_stencil_state.depth_func=ENUM_STENCIL_FUNCTION::ENUM_LESS;
		pd.blend_state.render_targets.resize(1);
		d.pso=g_render_rhi->CreateRenderPipelineState(pd);
		d.pso->CreateShaderResourceBinding(d.srb,false);
		d.srb->SetResource("op",op_buf);
		d.srb->SetResource("disp",disp_buf);   // binding 1 in VS
		d.srb->SetResource("norm",norm_buf);   // binding 2 in FS
		d.srb->FlushDescriptorWrites();
		delete vs; delete ps;
	},
	[=](CONST PD& d,CommandList* c){
		// params already uploaded by RecSpec each frame
		Vector<ClearValue> nc;
		Texture* d2=m_viewport->GetCurrentBackBufferDSV();
		c->SetRenderTarget({m_viewport->GetCurrentBackBufferRTV()},d2,nc,false);
		c->SetGraphicsPipeline(d.pso); c->SetShaderResourceBinding(d.srb);
		c->Draw(DrawAttribute{OCEAN_VERTS,1,0,0});
	}); p6->SetIsCullable(false);

	graph.Compile();
}
void RT::OnShutdown_Logic() {
	graph.Release();
	for(auto* s:{sInit,sSpec,sR,sC,sUnp,sExp}) delete s;
	for(auto* t:{h0,spec,tmp,spat,dmap,nmap}) delete t;
	delete op_buf; delete disp_buf; delete norm_buf;
}
void RT::OnUpdate(float dt) {
	auto& input = MXRender::Input::InputSystem::Get();
	g_scroll = input.GetScrollDelta();
	Float32 cx, cy2; input.GetMousePos(cx, cy2);
	Int ww, wh; m_window->GetFramebufferSize(ww, wh);
	Bool dn = input.IsMouseDown(0); // Left
	if(dn&&drag){cy+=(cx-(Float32)lcx)*0.005f; cp=glm::clamp(cp+(cy2-(Float32)lcy)*0.005f,0.06f,1.45f);}
	drag=dn; lcx=cx; lcy=cy2; cd=glm::clamp(cd*powf(0.9f,g_scroll),8.f,300.f);
	// Keyboard debug toggles via InputSystem
	Bool kn[7]; for(Int i=0;i<7;++i) kn[i]=false;
	kn[0]=input.IsKeyDown(MXRender::Input::EKey::K1); kn[1]=input.IsKeyDown(MXRender::Input::EKey::K2);
	kn[2]=input.IsKeyDown(MXRender::Input::EKey::K3); kn[3]=input.IsKeyDown(MXRender::Input::EKey::K4);
	kn[4]=input.IsKeyDown(MXRender::Input::EKey::K5);
	kn[5]=input.IsKeyDown(MXRender::Input::EKey::Up); kn[6]=input.IsKeyDown(MXRender::Input::EKey::Down);
	if(kn[0]&&!kp[0]){dm=0;ds=false;} if(kn[1]&&!kp[1]){dm=1;ds=false;}
	if(kn[2]&&!kp[2]){dm=2;ds=false;} if(kn[3]&&!kp[3]){dm=3;ds=false;}
	if(kn[4]&&!kp[4]){dm=2;ds=true;}
	if(kn[5]&&!kp[5]){ampl*=2.0f;fi=0;} if(kn[6]&&!kp[6]){ampl*=0.5f;fi=0;}
	for(Int i=0;i<7;++i) kp[i]=kn[i];
	if(ww<=0||wh<=0) return;
	glm::vec3 tgt(0,0,0);
	eye=tgt+cd*glm::vec3(cosf(cp)*sinf(cy),sinf(cp),cosf(cp)*cosf(cy));
	glm::mat4 view=glm::lookAt(eye,tgt,glm::vec3(0,1,0));
	glm::mat4 proj=glm::perspective(glm::radians(50.f),(Float32)ww/wh,0.1f,1000.f);
	proj[1][1]*=-1; vp=proj*view; ivp=glm::inverse(vp); ts+=dt;
}
void RT::OnRender() { graph.Execute(); }
int main() { Window w; RT r(w.GetPlatformWindow()); w.InitWindow(); w.Run(&r); system("pause"); return 0; }
