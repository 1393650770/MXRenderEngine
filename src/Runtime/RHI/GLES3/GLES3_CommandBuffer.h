#pragma once
#ifndef _GLES3_COMMANDBUFFER_
#define _GLES3_COMMANDBUFFER_

#if PLATFORM_GLES3

#include "RHI/RenderCommandList.h"
#include <GLES3/gl3.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

class GLES3_PipelineState;

// GLES3_CommandBuffer: GLES3 backend for CommandList.
// Single-threaded, immediate execution (bypass=true).
// GLES3 3.0 has no explicit barriers — TransitionTextureState/ResourceBarrier are no-ops.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(GLES3_CommandBuffer, public CommandList)
#pragma region METHOD
public:
	GLES3_CommandBuffer();
	VIRTUAL ~GLES3_CommandBuffer() OVERRIDE;

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

	// GLES3 implicit barriers: no-ops
	VIRTUAL void METHOD(TransitionTextureState)(Texture* texture, CONST ENUM_RESOURCE_STATE& required_state) OVERRIDE FINAL {}
	VIRTUAL void METHOD(ClearTexture)(Texture* texture, Vector<float> clear_value = Vector<float>(4, 0.0f)) OVERRIDE FINAL {}
	VIRTUAL void METHOD(ResourceBarrier)(ENUM_RESOURCE_STATE src_state, ENUM_RESOURCE_STATE dst_state) OVERRIDE FINAL {}

	// Vertex/Index buffer support (Phase 1+)
	VIRTUAL void METHOD(SetVertexBuffer)(Buffer* buffer, UInt32 slot, UInt32 stride, UInt32 offset) OVERRIDE FINAL;
	VIRTUAL void METHOD(SetIndexBuffer)(Buffer* buffer, UInt32 offset, Bool index32) OVERRIDE FINAL;
	VIRTUAL void METHOD(DrawIndexed)(UInt32 indexCount, UInt32 instanceCount, UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance) OVERRIDE FINAL;

	VIRTUAL Bool METHOD(WaitForFence)(float time_in_seconds_to_wait) OVERRIDE FINAL;

	// No ImGui in wasm: stubbed
	VIRTUAL void METHOD(BeginUI)() OVERRIDE FINAL {}
	VIRTUAL void METHOD(EndUI)() OVERRIDE FINAL {}
	VIRTUAL void METHOD(SetScissorEnable)(bool enable) OVERRIDE FINAL { (void)enable; }
	VIRTUAL void METHOD(SetScissor)(Int x, Int y, UInt32 w, UInt32 h) OVERRIDE FINAL { (void)x; (void)y; (void)w; (void)h; }
#pragma endregion

private:
	void ApplyViewportScissor(Texture* rt);
#pragma endregion

#pragma region MEMBER
protected:
	GLES3_PipelineState* m_current_graphics_pso = nullptr;

	// Current FBO state
	GLuint m_current_fbo = 0;
	UInt32 m_fbo_width = 0;
	UInt32 m_fbo_height = 0;

	// Vertex buffer state (bound VBOs by binding slot + stored stride)
	struct BoundVB { GLuint buf = 0; UInt32 stride = 0; UInt32 offset = 0; };
	BoundVB m_bound_vbs[4];  // up to 4 vertex buffer bindings

	// Index buffer state (for DrawIndexed)
	GLuint m_current_ibo = 0;
	GLenum m_current_index_type = GL_UNSIGNED_SHORT;
	UInt32 m_current_index_offset = 0;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _GLES3_COMMANDBUFFER_
