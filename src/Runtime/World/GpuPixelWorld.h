#pragma once
#ifndef _GPU_PIXEL_WORLD_
#define _GPU_PIXEL_WORLD_

#include "Core/ConstDefine.h"
#include "World/IPixelWorldSimulator.h"
#include "World/ITerrainEditSink.h"
#include "World/PixelWorldEventQueue.h"
#include "World/PixelWorldConstants.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Buffer;
class RenderPipelineState;
class ShaderResourceBinding;
class CommandList;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// GPU implementation of the pixel world (Phase 2): SSBO authoritative state,
// 6-dispatch simulation chain per tick (apply_edits -> clear_claim -> move_a
// -> move_b -> react_a -> react_b), checkerboard parity + atomic claim for
// move conflicts, localized reactions. All SRBs bound at init (never rebound
// at runtime); stage barriers via ComputeUtils::DispatchWithBarrier.
class GpuPixelWorld : public IPixelWorldSimulator, public ITerrainEditSink
{
#pragma region METHOD
public:
	GpuPixelWorld();
	VIRTUAL ~GpuPixelWorld() OVERRIDE;

	// Creates GPU resources, pipelines and SRBs. Must be called once before
	// any TickFrame / GetStateBuffer (Sample calls this in OnInitScene so the
	// display binding can reference state_a at init time).
	void METHOD(Init)();

	// ---- IPixelWorldSimulator ----
	VIRTUAL void METHOD(TickFrame)(CONST SimFrameContext& ctx) OVERRIDE;
	VIRTUAL UInt32 METHOD(GetWorldWidth)() CONST OVERRIDE { return kWorldW; }
	VIRTUAL UInt32 METHOD(GetWorldHeight)() CONST OVERRIDE { return kWorldH; }
	VIRTUAL UInt64 METHOD(GetTickCount)() CONST OVERRIDE { return tick_count_; }
	VIRTUAL void METHOD(Reset)(UInt32 seed) OVERRIDE;

	// ---- ITerrainEditSink ----
	VIRTUAL void METHOD(ApplyEdit)(CONST EditEvent& edit) OVERRIDE;

	// GPU authoritative state buffer (Phase 2 display source).
	RHI::Buffer* METHOD(GetStateBuffer)() CONST { return state_a_; }

protected:
	void METHOD(RunChain)(RHI::CommandList* cmd, UInt32 ticks, UInt32 frame_index);
	void METHOD(CreateResources)();
	void METHOD(CreatePipelinesAndBindings)();
	UInt32 METHOD(UploadParams)(RHI::CommandList* cmd, UInt32 frame_index);

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	// ---- GPU buffers ----
	RHI::Buffer* state_a_ = nullptr;        // authoritative state (Storage)
	RHI::Buffer* state_b_ = nullptr;        // ping buffer (Storage)
	RHI::Buffer* claim_ = nullptr;          // move claim (Storage)
	RHI::Buffer* sim_params_ = nullptr;     // Storage|Dynamic
	RHI::Buffer* material_props_ = nullptr; // Storage|Dynamic
	RHI::Buffer* events_[2] = { nullptr, nullptr };  // double-buffered events

	// ---- pipelines (owned by VK_PipelineStateManager, NEVER delete) ----
	RHI::RenderPipelineState* pso_clear_state_ = nullptr;
	RHI::RenderPipelineState* pso_apply_edits_ = nullptr;
	RHI::RenderPipelineState* pso_clear_claim_ = nullptr;
	RHI::RenderPipelineState* pso_move_a_ = nullptr;
	RHI::RenderPipelineState* pso_move_b_ = nullptr;
	RHI::RenderPipelineState* pso_react_a_ = nullptr;
	RHI::RenderPipelineState* pso_react_b_ = nullptr;

	// ---- SRBs (manual delete required) ----
	RHI::ShaderResourceBinding* srb_clear_state_ = nullptr;
	RHI::ShaderResourceBinding* srb_apply_[2] = { nullptr, nullptr };  // events parity
	RHI::ShaderResourceBinding* srb_clear_claim_ = nullptr;
	RHI::ShaderResourceBinding* srb_move_a_ = nullptr;
	RHI::ShaderResourceBinding* srb_move_b_ = nullptr;
	RHI::ShaderResourceBinding* srb_react_a_ = nullptr;
	RHI::ShaderResourceBinding* srb_react_b_ = nullptr;

	PixelWorldEventQueue event_queue_;
	UInt64 tick_count_ = 0;
	UInt32 seed_ = 0;
	Bool resources_created_ = false;
	Bool initialized_ = false;

private:
#pragma endregion
};  // class GpuPixelWorld

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _GPU_PIXEL_WORLD_