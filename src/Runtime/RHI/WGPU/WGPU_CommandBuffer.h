#pragma once
#ifndef _WGPU_COMMANDBUFFER_
#define _WGPU_COMMANDBUFFER_

#if PLATFORM_WGPU

#include "RHI/RenderCommandList.h"
#include <webgpu/webgpu.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)

class RenderPipelineState;
class ShaderResourceBinding;
class Texture;

MYRENDERER_BEGIN_NAMESPACE(WebGPU)

class WGPU_PipelineState;

// WGPU_CommandBuffer: WebGPU backend for CommandList.
// Single-threaded only (bypass=true): all commands execute immediately on the
// calling thread. WebGPU implicit barriers make TransitionTextureState /
// ResourceBarrier no-ops (Decision #12).
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(WGPU_CommandBuffer, public CommandList)
#pragma region METHOD
public:
	WGPU_CommandBuffer(WGPUDevice device);
	VIRTUAL ~WGPU_CommandBuffer() OVERRIDE;

	// Encoded command buffer: valid after End(), consumed by Submit().
	// Returns nullptr if Begin/End were not called or already consumed.
	WGPUCommandBuffer METHOD(GetEncodedCommandBuffer)() CONST { return m_cmd_buffer; }
	void METHOD(ConsumeEncodedCommandBuffer)();  // release after wgpuQueueSubmit

#pragma region RHI_INTERFACE
	VIRTUAL void METHOD(Begin)() OVERRIDE FINAL;
	VIRTUAL void METHOD(End)() OVERRIDE FINAL;

	VIRTUAL void METHOD(SetGraphicsPipeline)(RenderPipelineState* pipeline_state) OVERRIDE FINAL;
	VIRTUAL void METHOD(SetComputePipeline)(RenderPipelineState* pipeline_state) OVERRIDE FINAL;
	VIRTUAL void METHOD(SetRenderTarget)(CONST Vector<Texture*>& render_targets, Texture* depth_stencil, CONST Vector<ClearValue>& clear_values, Bool has_dsv_clear_value) OVERRIDE FINAL;
	VIRTUAL void METHOD(SetShaderResourceBinding)(ShaderResourceBinding* srb) OVERRIDE FINAL;

	VIRTUAL void METHOD(Draw)(CONST DrawAttribute& draw_attr) OVERRIDE FINAL;
	VIRTUAL void METHOD(Dispatch)(UInt32 groupX, UInt32 groupY, UInt32 groupZ) OVERRIDE FINAL;

	VIRTUAL void METHOD(SetPushConstants)(UInt32 offset, UInt32 size, const void* data) OVERRIDE FINAL;

	// WebGPU implicit barriers: both are no-ops (Decision #12)
	VIRTUAL void METHOD(TransitionTextureState)(Texture* texture, CONST ENUM_RESOURCE_STATE& required_state) OVERRIDE FINAL {}
	VIRTUAL void METHOD(ClearTexture)(Texture* texture, Vector<float> clear_value = Vector<float>(4, 0.0f)) OVERRIDE FINAL {}
	VIRTUAL void METHOD(ResourceBarrier)(ENUM_RESOURCE_STATE src_state, ENUM_RESOURCE_STATE dst_state) OVERRIDE FINAL {}

	VIRTUAL Bool METHOD(WaitForFence)(float time_in_seconds_to_wait) OVERRIDE FINAL;

	// No ImGui in wasm: stubbed
	VIRTUAL void METHOD(BeginUI)() OVERRIDE FINAL {}
	VIRTUAL void METHOD(EndUI)() OVERRIDE FINAL {}
	// WebGPU: scissoring not currently used for UI; stubbed
	VIRTUAL void METHOD(SetScissorEnable)(bool enable) OVERRIDE FINAL { (void)enable; }
	VIRTUAL void METHOD(SetScissor)(Int x, Int y, UInt32 w, UInt32 h) OVERRIDE FINAL { (void)x; (void)y; (void)w; (void)h; }
#pragma endregion

private:
	void EndActiveRenderPass();
	void EndActiveComputePass();

#pragma endregion

#pragma region MEMBER
protected:
	WGPUDevice m_device = nullptr;
	WGPUCommandEncoder m_encoder = nullptr;
	WGPURenderPassEncoder m_render_pass = nullptr;
	WGPUComputePassEncoder m_compute_pass = nullptr;
	WGPUCommandBuffer m_cmd_buffer = nullptr;

	WGPU_PipelineState* m_current_graphics_pso = nullptr;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
#endif // _WGPU_COMMANDBUFFER_
