#if PLATFORM_GLES3

#include "RHI/GLES3/GLES3_CommandBuffer.h"
#include "RHI/GLES3/GLES3_Texture.h"
#include "RHI/GLES3/GLES3_PipelineState.h"
#include "RHI/GLES3/GLES3_Buffer.h"
#include "RHI/GLES3/GLES3_ShaderResourceBinding.h"
#include "RHI/GLES3/GLES3_Utils.h"
#include "RHI/RenderRHI.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

GLES3_CommandBuffer::GLES3_CommandBuffer()
{
	bypass = true;  // Single-threaded: execute immediately, no recording
}

GLES3_CommandBuffer::~GLES3_CommandBuffer()
{
}

#pragma region LIFECYCLE

void GLES3_CommandBuffer::Begin()
{
	// Idempotent: GL operations go directly to the context.
	// The default framebuffer (0) is the initial render target.
}

void GLES3_CommandBuffer::End()
{
	// Commands are already executed. Flush for good measure.
	glFlush();
}

#pragma endregion

#pragma region RENDER_PASS

void GLES3_CommandBuffer::ApplyViewportScissor(Texture* rt)
{
	Int w = m_fbo_width, h = m_fbo_height;
	if (rt)
	{
		CONST auto& desc = rt->GetDesc();
		w = (Int)desc.width;
		h = (Int)desc.height;
	}
	glViewport(0, 0, w, h);
	glScissor(0, 0, w, h);
}

void GLES3_CommandBuffer::SetRenderTarget(CONST Vector<Texture*>& render_targets, Texture* depth_stencil, CONST Vector<ClearValue>& clear_values, Bool has_dsv_clear_value)
{
	// GLES3: bind a Framebuffer Object.
	// Phase 0 (HelloTriangle): render_targets[0] is the swapchain backbuffer
	// which uses the default framebuffer (0).

	GLuint fbo = 0;
	UInt32 fbo_w = 0, fbo_h = 0;

	if (render_targets.empty() || !render_targets[0])
	{
		// Bind default framebuffer (swapchain)
		fbo = 0;
		fbo_w = 1280; fbo_h = 720;  // will be overridden by viewport
	}
	else
	{
		auto* gl_tex = static_cast<GLES3_Texture*>(render_targets[0]);
		if (gl_tex->IsDefaultFramebuffer())
		{
			fbo = 0;
		}
		else if (gl_tex->GetGLFramebuffer())
		{
			fbo = gl_tex->GetGLFramebuffer();
		}
		CONST auto& desc = render_targets[0]->GetDesc();
		fbo_w = desc.width;
		fbo_h = desc.height;
	}

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);

	// Depth/stencil attachment
	if (depth_stencil)
	{
		auto* ds_tex = static_cast<GLES3_Texture*>(depth_stencil);
		GLuint ds_gl = ds_tex->GetGLTexture();
		CHECK_WITH_LOG(ds_gl == 0, "GLES3: depth/stencil texture has no GL object");
		// Attach to FBO if we created one; default FBO can't be modified
		if (fbo != 0)
		{
			glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, ds_gl, 0);
		}
	}

	// Clear
	GLbitfield clear_mask = 0;
	if (!clear_values.empty())
	{
		const Float32* c = clear_values[0].color;
		glClearColor(c[0], c[1], c[2], c[3]);
		clear_mask |= GL_COLOR_BUFFER_BIT;
	}
	if (has_dsv_clear_value && depth_stencil)
	{
		glClearDepthf(1.0f);
		clear_mask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
	}
	if (clear_mask)
	{
		glClear(clear_mask);
	}

	// Note: glClear() is only valid with glClearColor set first.
	// For per-pass clears, SetRenderTarget with clear values triggers glClear.
	// For load-only (no clear), we skip the clear.
	// RDG passes typically always specify clear on the first pass.

	m_current_fbo = fbo;
	m_fbo_width = fbo_w;
	m_fbo_height = fbo_h;
	glViewport(0, 0, (GLsizei)fbo_w, (GLsizei)fbo_h);
	glScissor(0, 0, (GLsizei)fbo_w, (GLsizei)fbo_h);
}

void GLES3_CommandBuffer::SetGraphicsPipeline(RenderPipelineState* pipeline_state)
{
	CHECK_WITH_LOG(pipeline_state == nullptr, "GLES3: SetGraphicsPipeline null pso");

	m_current_graphics_pso = static_cast<GLES3_PipelineState*>(pipeline_state);

	GLuint program = m_current_graphics_pso->GetGLProgram();
	CHECK_WITH_LOG(program == 0, "GLES3: PSO has no GL program");

	glUseProgram(program);

	// Bind VAO (0 = hardcoded triangle / no vertex input)
	GLuint vao = m_current_graphics_pso->GetVAO();
	glBindVertexArray(vao);

	// Apply pipeline state
	m_current_graphics_pso->ApplyState();
}

void GLES3_CommandBuffer::SetComputePipeline(RenderPipelineState* pipeline_state)
{
	// Phase C stub: GLES 3.0 doesn't support compute shaders
	(void)pipeline_state;
}

void GLES3_CommandBuffer::SetShaderResourceBinding(ShaderResourceBinding* srb)
{
	// HelloTriangle: SRB is empty (no resources bound). No-op.
	if (!srb) return;

	auto* gl_srb = static_cast<GLES3_ShaderResourceBinding*>(srb);
	gl_srb->ApplyBindings();
}

#pragma endregion

#pragma region DRAW

void GLES3_CommandBuffer::Draw(CONST DrawAttribute& draw_attr)
{
	CHECK_WITH_LOG(m_current_graphics_pso == nullptr, "GLES3: Draw called without pipeline");

	GLenum topology = TranslatePrimitiveTopology(m_current_graphics_pso->GetDesc().primitive_topology);
	glDrawArrays(topology, (GLint)draw_attr.firstVertex, (GLsizei)draw_attr.vertexCount);
}

void GLES3_CommandBuffer::Dispatch(UInt32 groupX, UInt32 groupY, UInt32 groupZ)
{
	// GLES 3.0 doesn't support compute shaders (requires ES 3.1+)
	(void)groupX; (void)groupY; (void)groupZ;
}

#pragma endregion

#pragma region VERTEX_INDEX

void GLES3_CommandBuffer::SetVertexBuffer(Buffer* buffer, UInt32 slot, UInt32 stride, UInt32 offset)
{
	auto* gl_buf = static_cast<GLES3_Buffer*>(buffer);
	if (!gl_buf || !gl_buf->GetGLBuffer()) return;
	glBindBuffer(GL_ARRAY_BUFFER, gl_buf->GetGLBuffer());
	// VertexAttribPointer is set during pipeline creation (VAO).
	// For Phase 1+, we'll store the layout and apply it here.
	(void)slot; (void)stride; (void)offset;
}

void GLES3_CommandBuffer::SetIndexBuffer(Buffer* buffer, UInt32 offset, Bool index32)
{
	auto* gl_buf = static_cast<GLES3_Buffer*>(buffer);
	if (!gl_buf || !gl_buf->GetGLBuffer()) return;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_buf->GetGLBuffer());
	m_current_index_type = index32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
	m_current_index_offset = offset;
}

void GLES3_CommandBuffer::DrawIndexed(UInt32 indexCount, UInt32 instanceCount, UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance)
{
	CHECK_WITH_LOG(m_current_graphics_pso == nullptr, "GLES3: DrawIndexed called without pipeline");

	GLenum topology = TranslatePrimitiveTopology(m_current_graphics_pso->GetDesc().primitive_topology);
	GLenum index_type = m_current_index_type ? m_current_index_type : GL_UNSIGNED_SHORT;
	size_t idx_offset = (size_t)firstIndex * (index_type == GL_UNSIGNED_INT ? 4 : 2);

	if (instanceCount > 1)
	{
		glDrawElementsInstanced(topology, (GLsizei)indexCount, index_type,
			(const void*)idx_offset, (GLsizei)instanceCount);
	}
	else
	{
		glDrawElements(topology, (GLsizei)indexCount, index_type, (const void*)idx_offset);
	}

	(void)vertexOffset; (void)firstInstance;
}

#pragma endregion

#pragma region MISC

void GLES3_CommandBuffer::SetPushConstants(UInt32 offset, UInt32 size, const void* data)
{
	// GLES3 has no push constants: use uniform buffers instead. No-op.
	(void)offset; (void)size; (void)data;
}

Bool GLES3_CommandBuffer::WaitForFence(float time_in_seconds_to_wait)
{
	// Single-threaded mode: all commands are synchronous, no fences needed.
	return true;
}

#pragma endregion

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
