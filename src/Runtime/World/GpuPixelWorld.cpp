#include "World/GpuPixelWorld.h"
#include "World/CellPacking.h"
#include "World/MaterialRegistry.h"
#include "Tool/ShaderLibrary.h"
#include "Tool/BufferUtils.h"
#include "Tool/ComputeUtils.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/RenderBuffer.h"
#include "RHI/RenderShader.h"
#include <cstring>
#include <algorithm>

using namespace MXRender::RHI;

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

namespace
{
	// Matches world_common.glsl SimParams (std430 layout: 8 x uint).
	struct SimParamsGpu
	{
		UInt32 tick = 0;
		UInt32 event_count = 0;
		UInt32 world_w = kWorldW;
		UInt32 world_h = kWorldH;
		UInt32 frame_index = 0;
		UInt32 pad0 = 0;
		UInt32 pad1 = 0;
		UInt32 pad2 = 0;
	};

	// Matches world_common.glsl MaterialProps: {phase,density,flammability,
	// lifetime_max} + vec4 color + {chance_permille, pad0, pad1, pad2}.
	struct MaterialPropsGpu
	{
		UInt32 phase = 0;
		UInt32 density = 0;
		UInt32 flammability = 0;
		UInt32 lifetime_max = 0;
		glm::vec4 color{ 0.0f };
		UInt32 chance_permille = 0;
		UInt32 pad0 = 0;
		UInt32 pad1 = 0;
		UInt32 pad2 = 0;
	};
	static_assert(sizeof(MaterialPropsGpu) == 48, "MaterialPropsGpu layout must match GLSL (48 bytes)");

	constexpr UInt32 kEventBufferCapacity = 16384;
	constexpr UInt32 kEventBufferBytes = (2 + kEventBufferCapacity) * sizeof(UInt32);  // count+pad+events
}

GpuPixelWorld::GpuPixelWorld() MYDEFAULT;

GpuPixelWorld::~GpuPixelWorld()
{
	// SRBs: manual delete. Buffers: raw delete. PSOs: owned by the manager.
	for (auto* srb : srb_apply_) { delete srb; srb = nullptr; }
	delete srb_clear_state_; srb_clear_state_ = nullptr;
	delete srb_clear_claim_; srb_clear_claim_ = nullptr;
	delete srb_move_a_; srb_move_a_ = nullptr;
	delete srb_move_b_; srb_move_b_ = nullptr;
	delete srb_react_a_; srb_react_a_ = nullptr;
	delete srb_react_b_; srb_react_b_ = nullptr;

	delete state_a_; state_a_ = nullptr;
	delete state_b_; state_b_ = nullptr;
	delete claim_; claim_ = nullptr;
	delete sim_params_; sim_params_ = nullptr;
	delete material_props_; material_props_ = nullptr;
	delete events_[0]; events_[0] = nullptr;
	delete events_[1]; events_[1] = nullptr;
}

void GpuPixelWorld::CreateResources()
{
	if (resources_created_)
		return;

	// World state: two ping-pong buffers of packed cells.
	BufferDesc desc;
	desc.type = ENUM_BUFFER_TYPE::Storage;
	desc.size = kWorldW * kWorldH * sizeof(UInt32);
	desc.stride = sizeof(UInt32);
	state_a_ = g_render_rhi->CreateBuffer(desc);
	state_b_ = g_render_rhi->CreateBuffer(desc);

	// Claim buffer (move stage only).
	claim_ = g_render_rhi->CreateBuffer(desc);

	// Dynamic host-visible buffers (reliable Map/Upload path).
	sim_params_ = Tool::BufferUtils::CreateDynamicParamBuffer(sizeof(SimParamsGpu));
	material_props_ = Tool::BufferUtils::CreateDynamicParamBuffer(kMaxMaterials * sizeof(MaterialPropsGpu));

	for (Int i = 0; i < 2; ++i)
		events_[i] = Tool::BufferUtils::CreateDynamicParamBuffer(kEventBufferBytes);

	resources_created_ = true;
}

void GpuPixelWorld::CreatePipelinesAndBindings()
{
	// Load all compute shaders.
	auto load_cs = [](CONST String& name) -> Shader* {
		return Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Compute, name);
	};
	Shader* cs_init = load_cs("Shader/world_clear_state.comp.spv");
	Shader* cs_apply = load_cs("Shader/world_apply_edits.comp.spv");
	Shader* cs_clear = load_cs("Shader/world_clear_claim.comp.spv");
	Shader* cs_mova = load_cs("Shader/world_movement_a.comp.spv");
	Shader* cs_movb = load_cs("Shader/world_movement_b.comp.spv");
	Shader* cs_reaca = load_cs("Shader/world_reaction_a.comp.spv");
	Shader* cs_reactb = load_cs("Shader/world_reaction_b.comp.spv");

	auto mk_pso = [](Shader* cs) -> RenderPipelineState* {
		RenderGraphiPipelineStateDesc desc{};
		desc.shaders[ENUM_SHADER_STAGE::Shader_Compute] = cs;
		desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		desc.raster_state.sample_count = 1;
		return g_render_rhi->CreateRenderPipelineState(desc);
	};
	pso_clear_state_ = mk_pso(cs_init);
	pso_apply_edits_ = mk_pso(cs_apply);
	pso_clear_claim_ = mk_pso(cs_clear);
	pso_move_a_ = mk_pso(cs_mova);
	pso_move_b_ = mk_pso(cs_movb);
	pso_react_a_ = mk_pso(cs_reaca);
	pso_react_b_ = mk_pso(cs_reactb);

	delete cs_init; delete cs_apply; delete cs_clear; delete cs_mova; delete cs_movb; delete cs_reaca; delete cs_reactb;

	// SRBs bound once at init (never rebound at runtime).
	auto bind = [](RenderPipelineState* pso, ShaderResourceBinding*& srb,
		Buffer* b0, Buffer* b1, Buffer* b2, Buffer* b3)
	{
		pso->CreateShaderResourceBinding(srb, false);
		if (b0) srb->SetResource("state_a", b0);
		if (b1) srb->SetResource("state_b", b1);
		if (b2) srb->SetResource("claim", b2);
		if (b3) srb->SetResource("props", b3);
		srb->FlushDescriptorWrites();
	};

	// clear_state: state_a + state_b (one-shot init)
	pso_clear_state_->CreateShaderResourceBinding(srb_clear_state_, false);
	srb_clear_state_->SetResource("state_a", state_a_);
	srb_clear_state_->SetResource("state_b", state_b_);
	srb_clear_state_->FlushDescriptorWrites();

	// apply_edits: events (parity) + state_a
	for (Int i = 0; i < 2; ++i)
	{
		pso_apply_edits_->CreateShaderResourceBinding(srb_apply_[i], false);
		srb_apply_[i]->SetResource("events", events_[i]);
		srb_apply_[i]->SetResource("cells", state_a_);
		srb_apply_[i]->FlushDescriptorWrites();
	}

	// clear_claim: claim only
	pso_clear_claim_->CreateShaderResourceBinding(srb_clear_claim_, false);
	srb_clear_claim_->SetResource("claim", claim_);
	srb_clear_claim_->FlushDescriptorWrites();

	// move_a / move_b: state_a (readonly) + state_b (write) + claim + props
	bind(pso_move_a_, srb_move_a_, state_a_, state_b_, claim_, material_props_);
	bind(pso_move_b_, srb_move_b_, state_a_, state_b_, claim_, material_props_);

	// react_a / react_b: state_b (readonly) + state_a (write) + props + params
	for (Int i = 0; i < 2; ++i)
	{
		RenderPipelineState* pso = (i == 0) ? pso_react_a_ : pso_react_b_;
		ShaderResourceBinding*& srb = (i == 0) ? srb_react_a_ : srb_react_b_;
		pso->CreateShaderResourceBinding(srb, false);
		srb->SetResource("state_b", state_b_);
		srb->SetResource("state_a", state_a_);
		srb->SetResource("props", material_props_);
		srb->SetResource("params", sim_params_);
		srb->FlushDescriptorWrites();
	}

	// Initial state: all empty via the one-shot clear_state dispatch (runs
	// once on the first TickFrame). Phase 4 chunked generation will rebuild
	// baseline terrain from the seed.
	initialized_ = true;
}

UInt32 GpuPixelWorld::UploadParams(RHI::CommandList* cmd, UInt32 frame_index)
{
	(void)cmd;
	SimParamsGpu params;
	params.tick = (UInt32)tick_count_;
	params.event_count = event_queue_.GetPendingCount();
	params.world_w = kWorldW;
	params.world_h = kWorldH;
	params.frame_index = frame_index;
	Tool::BufferUtils::Upload(sim_params_, &params, sizeof(params));

	// Upload material props once (registry is static after init in Phase 2).
	static Bool props_uploaded = false;
	if (!props_uploaded)
	{
		Vector<MaterialPropsGpu> props(kMaxMaterials);
		UInt32 count = MaterialRegistry::GetCount();
		for (UInt32 i = 0; i < count; ++i)
		{
			const MaterialDef& def = MaterialRegistry::Get((UInt8)i);
			MaterialPropsGpu& p = props[i];
			p.phase = (UInt32)def.phase;
			p.density = def.density;
			p.flammability = def.flammability;
			p.lifetime_max = def.lifetime_max;
			p.color = glm::vec4(def.color, 1.0f);
			p.chance_permille = def.chance_permille;
		}
		Tool::BufferUtils::Upload(material_props_, props.data(), (UInt32)(props.size() * sizeof(MaterialPropsGpu)));
		props_uploaded = true;
	}

	return params.event_count;
}

void GpuPixelWorld::RunChain(RHI::CommandList* cmd, UInt32 ticks, UInt32 frame_index)
{
	for (UInt32 t = 0; t < ticks; ++t)
	{
		// Stage 1: apply edits into A (skip if no events).
		UInt32 event_count = event_queue_.GetPendingCount();
		if (event_count > 0)
		{
			event_queue_.FlushTo(events_[frame_index & 1], kEventBufferCapacity);
			Tool::ComputeUtils::DispatchWithBarrier(cmd, pso_apply_edits_, srb_apply_[frame_index & 1],
				(kEventBufferCapacity + 63) / 64, 1, 1);
		}

		// Stage 2: clear claim.
		Tool::ComputeUtils::DispatchWithBarrier(cmd, pso_clear_claim_, srb_clear_claim_,
			(kWorldW * kWorldH + 255) / 256, 1, 1);

		// Stage 3+4: movement (checkerboard parity).
		Tool::ComputeUtils::DispatchWithBarrier(cmd, pso_move_a_, srb_move_a_,
			(kWorldW * kWorldH + 255) / 256, 1, 1);
		Tool::ComputeUtils::DispatchWithBarrier(cmd, pso_move_b_, srb_move_b_,
			(kWorldW * kWorldH + 255) / 256, 1, 1);

		// Stage 5+6: reactions (localized, parity split).
		Tool::ComputeUtils::DispatchWithBarrier(cmd, pso_react_a_, srb_react_a_,
			(kWorldW * kWorldH + 255) / 256, 1, 1);
		Tool::ComputeUtils::DispatchWithBarrier(cmd, pso_react_b_, srb_react_b_,
			(kWorldW * kWorldH + 255) / 256, 1, 1);

		++tick_count_;
	}
}

void GpuPixelWorld::Init()
{
	if (resources_created_)
		return;
	CreateResources();
	CreatePipelinesAndBindings();
}

void GpuPixelWorld::TickFrame(CONST SimFrameContext& ctx)
{
	if (ctx.cmd == nullptr)
		return;
	if (!resources_created_)
		Init();

	// Chain gate: run at least once if there are unconsumed events from the
	// previous frame (prevents dropping edits at high fps: pending_ticks may
	// be 0 while events are still in the GPU buffer).
	UInt32 chain_ticks = ctx.pending_ticks;
	if (chain_ticks == 0 && event_queue_.last_flushed_ > 0)
		chain_ticks = 1;

	UploadParams(ctx.cmd, ctx.frame_index);

	// One-shot init on first frame.
	if (!initialized_)
	{
		Tool::ComputeUtils::DispatchWithBarrier(ctx.cmd, pso_clear_state_, srb_clear_state_,
			(kWorldW * kWorldH + 255) / 256, 1, 1);
		initialized_ = true;
	}

	if (chain_ticks > 0)
		RunChain(ctx.cmd, chain_ticks, ctx.frame_index);
}

void GpuPixelWorld::ApplyEdit(CONST EditEvent& edit)
{
	event_queue_.Enqueue(edit);
}

void GpuPixelWorld::Reset(UInt32 seed)
{
	seed_ = seed;
	tick_count_ = 0;
	event_queue_.Reset();
	// The world is reset to empty by the sim (no persistent state carried).
	// Phase 4 chunked generation will rebuild baseline terrain from the seed.
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender