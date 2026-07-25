#include "Render/Core/RenderGraphSerializer.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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
#include "Asset/TextureAsset.h"
#include "Render/Core/BindlessMaterialData.h"
#include "RHI/Vulkan/VK_BindlessManager.h"
#include "Render/Core/RenderGraphDefinition.h"
using namespace MXRender::Asset;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;
using namespace MXRender;

Vector<UInt32> ReadShader(CONST String& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	CHECK_WITH_LOG(!file.is_open(), " App Error: fail to open the shader file! ")
	size_t fileSize = (size_t)file.tellg();
	Vector<UInt32> buffer(fileSize / sizeof(UInt32));
	file.seekg(0); file.read((char*)buffer.data(), fileSize); file.close();
	return std::move(buffer);
}

static float g_cam_angle = 0.0f;
static float g_cam_distance = 3.0f;

// Pass data shared between setup and execute
struct SkyboxPassData : public RenderGraphPassDataBase {
	RenderPipelineState* pipeline = nullptr;
	ShaderResourceBinding* srb = nullptr;
	TextureAsset* cubemap_asset = nullptr;
	void Release() {
		if (srb) { delete srb; srb = nullptr; }
		if (cubemap_asset) { delete cubemap_asset; cubemap_asset = nullptr; }
	}
	VIRTUAL ~SkyboxPassData() { Release(); }
};

struct PBRPassData : public RenderGraphPassDataBase {
	RenderPipelineState* pipeline = nullptr;
	ShaderResourceBinding* srb = nullptr;
	TextureAsset* basecolor=nullptr, *normal=nullptr, *aorm=nullptr, *cubemap=nullptr, *irradiance=nullptr;
	Buffer* mvp_ubo=nullptr, *camera_ubo=nullptr, *material_ubo=nullptr;
	BindlessSlotHandle bc_idx, n_idx, a_idx, lut_idx;
	BindlessCubeSlotHandle cm_idx, irr_idx;
	RHI::BindlessManager* bindless_mgr = nullptr;
	Texture* fallback_lut = nullptr;
	void Release() {
		if (srb) { delete srb; srb = nullptr; }
		if (basecolor) { delete basecolor; basecolor = nullptr; }
		if (normal)    { delete normal; normal = nullptr; }
		if (aorm)      { delete aorm; aorm = nullptr; }
		if (cubemap)   { delete cubemap; cubemap = nullptr; }
		if (irradiance){ delete irradiance; irradiance = nullptr; }
		if (mvp_ubo)   { delete mvp_ubo; mvp_ubo = nullptr; }
		if (camera_ubo){ delete camera_ubo; camera_ubo = nullptr; }
		if (material_ubo){ delete material_ubo; material_ubo = nullptr; }
		if (fallback_lut) { delete fallback_lut; fallback_lut = nullptr; }
	}
	VIRTUAL ~PBRPassData() { Release(); }
};

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RenderTest, public MXRender::RenderInterface)
public:
	RenderTest(PlatformWindow* w) : m_window(w) {}
	RenderTest() MYDEFAULT;
	~RenderTest() MYDEFAULT;
	void OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport) OVERRIDE FINAL;
	void OnShutdown_Logic() OVERRIDE FINAL;
	void OnUpdate(float dt) OVERRIDE FINAL {}
	void OnRender() OVERRIDE FINAL { GetRenderGraph().Execute(); }
	PlatformWindow* GetPlatformWindow() { return m_window; }
	RHI::Viewport* m_viewport = nullptr;
protected:
	PlatformWindow* m_window;

	// Resource pointers for serialization
	RenderGraphResource<RHI::TextureDesc, RHI::Texture>* rt_resource = nullptr;
	RenderGraphResource<RHI::TextureDesc, RHI::Texture>* ds_resource = nullptr;
	Vector<RenderGraphResource<RHI::TextureDesc, RHI::Texture>*> tex_resources;
	Vector<RenderGraphResource<RHI::BufferDesc, RHI::Buffer>*> buffer_resources;
MYRENDERER_END_CLASS

void RenderTest::OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport)
{
	m_window = in_window;
	m_viewport = in_viewport;
	std::cout << "=== Bindless PBR ===" << std::endl;
	CommandList* cmd_list = RHIGetImmediateCommandList();
	auto* vp = m_viewport;

	// Step 1: Register external BackBuffer + DepthStencil
	RHI::Texture* bb = vp->GetCurrentBackBufferRTV();
	RHI::Texture* ds = vp->GetCurrentBackBufferDSV();
	rt_resource = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
		"BackBuffer", bb->GetTextureDesc(), bb);
	if (ds) ds_resource = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
		"DepthStencil", ds->GetTextureDesc(), ds);

	// Step 2: Load all textures and register as retained resources
	auto RegTex = [&](const char* name, const char* path) {
		auto* asset = new TextureAsset(path);
		RHI::Texture* tex = asset->GetTexture();
		RHI::TextureDesc desc; if (tex) desc = tex->GetTextureDesc();
		auto* res = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(name, desc, tex);
		res->SetFilePath(path);
		tex_resources.push_back(res);
		return asset;
	};
	auto* sky_asset   = RegTex("SkyboxCubemap",   "Texture/Skybox/bolonga_lod.dds");
	auto* bc_asset    = RegTex("BasecolorTex",    "Texture/pbr_stone/pbr_stone_base_color.dds");
	auto* n_asset     = RegTex("NormalTex",       "Texture/pbr_stone/pbr_stone_normal.dds");
	auto* aorm_asset  = RegTex("AORMTex",         "Texture/pbr_stone/pbr_stone_aorm.dds");
	auto* cm_asset_db = RegTex("CubemapTex",      "Texture/Skybox/bolonga_lod.dds");
	auto* irr_asset   = RegTex("IrradianceTex",   "Texture/Skybox/bolonga_irr.dds");

	// Create fallback IBL LUT texture
	TextureDesc td; td.format=ENUM_TEXTURE_FORMAT::BGRA8; td.width=2; td.height=2;
	td.type=ENUM_TEXTURE_TYPE::ENUM_TYPE_2D; td.usage=ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
	Texture* fl = g_render_rhi->CreateTexture(td);
	Char w[16]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
	TextureDataPayload lp; lp.data=Vector<Char>(w,w+16); lp.width=2; lp.height=2;
	lp.format=ENUM_TEXTURE_FORMAT::BGRA8; lp.type=ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	lp.layer_count=1; lp.mip_level=1; lp.depth=1; fl->UpdateTextureData(lp);
	auto* lut_res = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
		"FallbackLUT", td, fl);
	tex_resources.push_back(lut_res);

	// Register UBOs
	auto RegBuf = [&](const char* name) {
		BufferDesc ub; ub.type=ENUM_BUFFER_TYPE::Uniform; ub.size=256;
		Buffer* b = g_render_rhi->CreateBuffer(ub);
		auto* res = graph.AddRetainedResource<RHI::BufferDesc, RHI::Buffer>(name, ub, b);
		buffer_resources.push_back(res);
		return b;
	};
	Buffer* mvp_buf    = RegBuf("MVP_UBO");
	Buffer* camera_buf = RegBuf("Camera_UBO");
	Buffer* mat_buf    = RegBuf("Material_UBO");

	// wait for async texture loads
	auto Wait = [](TextureAsset* t, const char* n){while(!t->GetTexture()){}std::cout<<"  "<<n<<std::endl;};
	Wait(bc_asset,"basecolor"); Wait(n_asset,"normal"); Wait(aorm_asset,"aorm");
	Wait(cm_asset_db,"cubemap"); Wait(irr_asset,"irradiance"); Wait(sky_asset,"skybox");

	// Bindless registration (global singleton, not RDG-manageable)
	auto* bindless_mgr = RHIGetBindlessManager();

	// Step 3: SkyboxPass
	auto* sky_pass = graph.AddRenderPass<SkyboxPassData>("SkyboxPass", &graph, cmd_list,
	[&](SkyboxPassData& d, RenderGraphPassBuilder& builder, CommandList*)
	{
		builder.Read(tex_resources[0]); // SkyboxCubemap
		builder.Write(rt_resource);
		if (ds_resource) builder.Write(ds_resource);

		ShaderDesc sd; ShaderDataPayload sp;
		sd.shader_type = ENUM_SHADER_STAGE::Shader_Vertex;
		sp.data = ReadShader("Shader/skybox_test.vert.spv"); Shader* sv = RHICreateShader(sd, sp);
		sd.shader_type = ENUM_SHADER_STAGE::Shader_Pixel;
		sp.data = ReadShader("Shader/skybox_test.frag.spv"); Shader* sps = RHICreateShader(sd, sp);

		RenderGraphiPipelineStateDesc pd{};
		pd.shaders[0]=sv; pd.shaders[1]=sps;
		pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		Vector<Texture*> rtvs = { bb }; pd.render_targets = rtvs; pd.depth_stencil_view = ds;
		pd.raster_state.sample_count = 1; pd.blend_state.render_targets.resize(1);
		d.pipeline = g_render_rhi->CreateRenderPipelineState(pd);
		d.pipeline->CreateShaderResourceBinding(d.srb, true);
		d.cubemap_asset = sky_asset;
		delete sv; delete sps;
	},
	[=](CONST SkyboxPassData& d, CommandList* cmd)
	{
		if (!d.cubemap_asset->GetTexture()) return;
		auto* vp = m_viewport;
		Vector<ClearValue> cc; cc.push_back({0.2f,0.2f,0.3f,1.0f});
		Texture* dsv = vp->GetCurrentBackBufferDSV();
		if(dsv) cc.push_back({1.0f, 0});
		Vector<Texture*> rtvs = { vp->GetCurrentBackBufferRTV() };
		cmd->SetRenderTarget(rtvs, dsv, cc, dsv != nullptr);
		cmd->SetGraphicsPipeline(d.pipeline);
		d.srb->SetResource("cubemap_sampler", d.cubemap_asset->GetTexture());
		cmd->SetShaderResourceBinding(d.srb);
		cmd->Draw(DrawAttribute{6,1,0,0});
	});
	sky_pass->SetIsCullable(false);
	sky_pass->SetShaderPath("Shader/skybox_test");

	// Step 4: PBRPass (bindless rendering)
	auto* pbr_pass = graph.AddRenderPass<PBRPassData>("PBRPass", &graph, cmd_list,
	[&](PBRPassData& d, RenderGraphPassBuilder& builder, CommandList*)
	{
		builder.Read(rt_resource); // Read skybox result from backbuffer
		builder.Write(rt_resource);
		if (ds_resource) builder.Write(ds_resource);
		for (auto* tr : tex_resources) builder.Read(tr);
		for (auto* br : buffer_resources) builder.Read(br);

		ShaderDesc sd; ShaderDataPayload sp;
		sd.shader_type = ENUM_SHADER_STAGE::Shader_Vertex;
		sp.data = ReadShader("Shader/pbr_bindless_demo.vert.spv"); Shader* pv = RHICreateShader(sd, sp);
		sd.shader_type = ENUM_SHADER_STAGE::Shader_Pixel;
		sp.data = ReadShader("Shader/pbr_mesh_bindless.frag.spv"); Shader* pps = RHICreateShader(sd, sp);

		RenderGraphiPipelineStateDesc pd{};
		pd.shaders[0]=pv; pd.shaders[1]=pps;
		pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		Vector<Texture*> rtvs = { bb }; pd.render_targets = rtvs; pd.depth_stencil_view = ds;
		pd.raster_state.sample_count = 1; pd.raster_state.cull_mode = ENUM_RASTER_CULLMODE::None;
		pd.depth_stencil_state.depth_test_enable = true;
		pd.depth_stencil_state.depth_write_enable = true;
		pd.blend_state.render_targets.resize(1);
		d.pipeline = g_render_rhi->CreateRenderPipelineState(pd);
		d.pipeline->CreateShaderResourceBinding(d.srb, false);
		delete pv; delete pps;

		d.basecolor = bc_asset; d.normal = n_asset; d.aorm = aorm_asset;
		d.cubemap = cm_asset_db; d.irradiance = irr_asset;
		d.mvp_ubo = mvp_buf; d.camera_ubo = camera_buf; d.material_ubo = mat_buf;
		d.bindless_mgr = bindless_mgr;
		d.fallback_lut = fl;

		// Bindless indices
		d.bc_idx = bindless_mgr->AllocateTexture2DSlot(bc_asset->GetTexture());
		d.n_idx  = bindless_mgr->AllocateTexture2DSlot(n_asset->GetTexture());
		d.a_idx  = bindless_mgr->AllocateTexture2DSlot(aorm_asset->GetTexture());
		d.cm_idx = bindless_mgr->AllocateTextureCubeSlot(cm_asset_db->GetTexture());
		d.irr_idx= bindless_mgr->AllocateTextureCubeSlot(irr_asset->GetTexture());
		d.lut_idx= bindless_mgr->AllocateTexture2DSlot(fl);

		// Material UBO
		BindlessMaterialDataGPU mat{}; mat.basecolorIndex=d.bc_idx.GetIndex(); mat.normalIndex=d.n_idx.GetIndex();
		mat.aormIndex=d.a_idx.GetIndex(); mat.cubemapIndex=d.cm_idx.GetIndex(); mat.irradianceIndex=d.irr_idx.GetIndex();
		mat.iblLutIndex=d.lut_idx.GetIndex(); mat.metallicFactor=0; mat.roughnessFactor=0.5f;
		void* mp=RHIMapBuffer(d.material_ubo,ENUM_MAP_TYPE::Write,ENUM_MAP_FLAG::None);
		memcpy(mp,&mat,sizeof(mat)); RHIUnmapBuffer(d.material_ubo);
		std::cout << "  Bindless ready." << std::endl;
	},
	[=](CONST PBRPassData& d, CommandList* cmd)
	{
		g_cam_angle += 0.005f;
		float cx=sin(g_cam_angle)*g_cam_distance, cz=cos(g_cam_angle)*g_cam_distance;
		glm::vec3 eye(cx,0.8f,cz);
		glm::mat4 view=glm::lookAt(eye,glm::vec3(0,0,0),glm::vec3(0,1,0));
		glm::mat4 proj=glm::perspective(glm::radians(60.f),1280.f/960.f,0.1f,100.f);
		{
			glm::mat4 m=glm::rotate(glm::mat4(1),glm::radians(-20.f),glm::vec3(1,0,0));
			void* p=RHIMapBuffer(d.mvp_ubo,ENUM_MAP_TYPE::Write,ENUM_MAP_FLAG::None);
			glm::mat4* a=static_cast<glm::mat4*>(p); a[0]=m;a[1]=view;a[2]=proj;
			RHIUnmapBuffer(d.mvp_ubo);
		}
		{
			struct C_{glm::vec3 p;float _0;glm::vec3 d;float _1;};
			void* p=RHIMapBuffer(d.camera_ubo,ENUM_MAP_TYPE::Write,ENUM_MAP_FLAG::None);
			C_* c=static_cast<C_*>(p); c->p=eye; c->d=glm::normalize(glm::vec3(0,0,0)-eye);
			c->_0=c->_1=0; RHIUnmapBuffer(d.camera_ubo);
		}

		Vector<ClearValue> nc;
		Vector<Texture*> rtvs = { vp->GetCurrentBackBufferRTV() };
		Texture* dsv = vp->GetCurrentBackBufferDSV();
		cmd->SetRenderTarget(rtvs, dsv, nc, dsv != nullptr);
		cmd->SetGraphicsPipeline(d.pipeline);
		d.srb->SetResource("mvp", d.mvp_ubo);
		d.srb->SetResource("cameraData", d.camera_ubo);
		d.srb->SetResource("g_Material", d.material_ubo);
		cmd->SetShaderResourceBinding(d.srb);
		cmd->Draw(DrawAttribute{6,1,0,0});
	});
	pbr_pass->SetIsCullable(false);
	pbr_pass->SetShaderPath("Shader/pbr_mesh_bindless");

	graph.Compile();
}

void RenderTest::OnShutdown_Logic()
{
	Render::RenderGraphDefinition def;
	def.graph_name = "Bindless";
	def.version = 2;

	for (auto& res : graph.GetResources())
	{
		Render::RDGResourceDef rd;
		rd.name = res->GetName();
		rd.is_transient = res->GetIsTransient();
		rd.file_path = res->GetFilePath();
		rd.lifetime = res->GetIsTransient() ? RDGResourceLifetime::Transient : RDGResourceLifetime::External;
		if (auto* tex = res->GetAsTexture())
		{
			rd.desc = tex->GetTextureDesc();
			rd.is_depth_stencil = ((UInt32)tex->GetTextureDesc().usage & 16) != 0;
		}
		else if (res->GetAsBuffer())
		{
			rd.desc = res->GetAsBuffer()->GetBufferDesc();
			// --   Read buffer content for serialization
			if (RHI::Buffer* buf = res->GetAsBuffer()) {
				UInt32 sz = buf->GetBufferDesc().size;
				void* p = RHIMapBuffer(buf, ENUM_MAP_TYPE::Read, ENUM_MAP_FLAG::None);
				if (p) { rd.buffer_data.assign((UInt8*)p, (UInt8*)p + sz); RHIUnmapBuffer(buf); }
			}
		}
		def.resources.push_back(rd);
	}

	for (auto& pass : graph.GetPasses())
	{
		Render::RDGPassDef pd;
		pd.name = pass->GetName();
		pd.pass_kind = Render::RDGPassKind::Graphics;
		pd.shader_path = pass->GetShaderPath();
		for (auto* r : pass->GetReadResources())  pd.read_resources.push_back(r->GetName());
		for (auto* w : pass->GetWriteResources()) pd.write_resources.push_back(w->GetName());
		for (auto* c : pass->GetCreateResources()) pd.create_resources.push_back(c->GetName());
		def.passes.push_back(pd);
	}

	Render::RenderGraphSerializer::SaveGraph(def, "bindless.rgraph.json");
	std::cout << "[Sample] Graph saved to bindless.rgraph.json" << std::endl;

		graph.Release();
}

int main() {
	Window m_window; RenderTest render(m_window.GetPlatformWindow());
	m_window.InitWindow(); m_window.Run(&render);
	system("pause"); return 0;
}
