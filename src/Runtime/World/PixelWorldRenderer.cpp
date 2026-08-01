#include "World/PixelWorldRenderer.h"
#include "World/PixelWorld.h"
#include "World/MaterialRegistry.h"
#include "Tool/ShaderLibrary.h"
#include "Tool/BufferUtils.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/RenderBuffer.h"
#include "RHI/RenderTexture.h"

using namespace MXRender::RHI;

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

namespace
{
	struct DisplayParams
	{
		UInt32 world_w = kWorldW;
		UInt32 world_h = kWorldH;
	};

	struct DisplayPassData : public Render::RenderGraphPassDataBase
	{
		VIRTUAL ~DisplayPassData() MYDEFAULT;
		VIRTUAL void METHOD(Release)() OVERRIDE {}
	};
}

PixelWorldRenderer::PixelWorldRenderer() MYDEFAULT;

PixelWorldRenderer::~PixelWorldRenderer()
{
	Shutdown();
}

void PixelWorldRenderer::Init()
{
	// Mirror: full world as packed UInt32 (W*H*4 bytes).
	mirror_ = Tool::BufferUtils::CreateDynamicParamBuffer(kWorldW * kWorldH * sizeof(UInt32));

	// Palette: one vec4 per material slot.
	palette_ = Tool::BufferUtils::CreateDynamicParamBuffer(kMaxMaterials * sizeof(glm::vec4));

	// Params uniform block (world size).
	params_ = Tool::BufferUtils::CreateDynamicParamBuffer(sizeof(DisplayParams));
	DisplayParams p;
	p.world_w = kWorldW;
	p.world_h = kWorldH;
	Tool::BufferUtils::Upload(params_, &p, sizeof(p));

	UploadPalette();
}

void PixelWorldRenderer::Shutdown()
{
	delete srb_mirror_; srb_mirror_ = nullptr;   // SRB: manual delete required
	delete srb_gpu_; srb_gpu_ = nullptr;         // PSO: owned by PipelineStateManager, NEVER delete
	delete mirror_; mirror_ = nullptr;
	delete palette_; palette_ = nullptr;
	delete params_; params_ = nullptr;
}

void PixelWorldRenderer::UploadPalette()
{
	if (!palette_)
		return;
	UInt32 count = MaterialRegistry::GetCount();
	Vector<glm::vec4> colors(kMaxMaterials, glm::vec4(0.0f));
	for (UInt32 i = 0; i < count; ++i)
	{
		const MaterialDef& def = MaterialRegistry::Get((UInt8)i);
		colors[i] = glm::vec4(def.color, 1.0f);
	}
	Tool::BufferUtils::Upload(palette_, colors.data(), (UInt32)(colors.size() * sizeof(glm::vec4)));
}

void PixelWorldRenderer::UploadMirror(CONST PixelWorld& world)
{
	if (!mirror_)
		return;
	const Vector<UInt32>& cells = world.GetCells();
	Tool::BufferUtils::Upload(mirror_, cells.data(), (UInt32)(cells.size() * sizeof(UInt32)));
}

void PixelWorldRenderer::CreateDisplayBindings(RHI::Buffer* gpu_state)
{
	// Phase 2: bind the GPU authoritative state as an alternative display source.
	// Both SRBs are created and flushed at init time; SetDisplaySource only
	// selects which one is bound (never re-binds descriptors at runtime).
	if (gpu_state && !srb_gpu_)
	{
		pso_display_->CreateShaderResourceBinding(srb_gpu_, false);
		srb_gpu_->SetResource("world_cells", gpu_state);
		srb_gpu_->SetResource("world_palette", palette_);
		srb_gpu_->SetResource("world_params", params_);
		srb_gpu_->FlushDescriptorWrites();
	}
}

void PixelWorldRenderer::SetDisplaySource(Bool use_gpu)
{
	use_gpu_source_ = use_gpu;
}

void PixelWorldRenderer::RegisterDisplayPass(
	Render::RenderGraph* graph,
	Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>* backbuffer_resource,
	RHI::Texture* depth_stencil,
	RHI::CommandList* immediate_cmd)
{
	CHECK_WITH_LOG(graph == nullptr || backbuffer_resource == nullptr, "PixelWorldRenderer: null graph/backbuffer")

	// Load shaders and build the PSO once.
	Shader* vs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Vertex,
		"Shader/fullscreen.vert.spv");
	Shader* ps = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Pixel,
		"Shader/pixel_world_display.frag.spv");

	RenderGraphiPipelineStateDesc pd{};
	pd.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
	pd.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps;
	pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
	pd.render_targets = { backbuffer_resource->GetActual() };
	// CRITICAL (2026-08): blend_state.render_targets must be sized to the
	// render target count. Without it VK_PipelineState builds an empty
	// VkPipelineColorBlendStateCreateInfo (attachmentCount=0) while dynamic
	// rendering uses colorAttachmentCount=1 -> VUID violation -> pipeline
	// creation fails -> bind of VK_NULL_HANDLE is ignored -> black screen.
	pd.blend_state.render_targets.resize(1);
	pd.raster_state.sample_count = 1;
	pd.raster_state.cull_mode = ENUM_RASTER_CULLMODE::None;
	pso_display_ = g_render_rhi->CreateRenderPipelineState(pd);
	delete vs;
	delete ps;

	// Init-time SRB binding (mirror source).
	pso_display_->CreateShaderResourceBinding(srb_mirror_, false);
	srb_mirror_->SetResource("world_cells", mirror_);
	srb_mirror_->SetResource("world_palette", palette_);
	srb_mirror_->SetResource("world_params", params_);
	srb_mirror_->FlushDescriptorWrites();

	auto* pass = graph->AddRenderPass<DisplayPassData>("PixelWorldDisplay", graph, immediate_cmd,
	[&](DisplayPassData& data, Render::RenderGraphPassBuilder& builder, CommandList* cmd)
	{
		builder.Write(backbuffer_resource);
	},
	[=](CONST DisplayPassData& data, CommandList* cmd)
	{
		// Select display source at execution time (SRB choice only).
		ShaderResourceBinding* srb = use_gpu_source_ ? srb_gpu_ : srb_mirror_;
		CHECK_WITH_LOG(srb == nullptr, "PixelWorldRenderer: display SRB not bound")

		// Standard render-target setup (same pattern as Fluid2D): fetch the
		// current backbuffer/DSV each execute (swapchain may rebuild), clear
		// with the texture's own clear values.
		Vector<Texture*> rtvs = { backbuffer_resource->GetActual() };
		Vector<ClearValue> clears;
		for (auto rtv : rtvs)
			clears.push_back(rtv->GetTextureDesc().clear_value);
		if (depth_stencil)
			clears.push_back(depth_stencil->GetTextureDesc().clear_value);
		cmd->SetRenderTarget(rtvs, depth_stencil, clears, depth_stencil != nullptr);
		cmd->SetGraphicsPipeline(pso_display_);
		cmd->SetShaderResourceBinding(srb);

		DrawAttribute draw{};
		draw.vertexCount = 3;
		draw.instanceCount = 1;
		cmd->Draw(draw);
	});
	pass->SetIsCullable(false);
	pass->SetShaderPath("Shader/pixel_world_display");
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender