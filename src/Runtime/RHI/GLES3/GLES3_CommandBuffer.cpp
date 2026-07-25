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
		CONST auto& desc = rt->GetTextureDesc();
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
		CONST auto& desc = render_targets[0]->GetTextureDesc();
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
	if (!srb) return;

	GLuint program = m_current_graphics_pso ? m_current_graphics_pso->GetGLProgram() : 0;
	auto* gl_srb = static_cast<GLES3_ShaderResourceBinding*>(srb);
	gl_srb->ApplyBindings(program);
}

#pragma endregion

#pragma region DRAW

void GLES3_CommandBuffer::Draw(CONST DrawAttribute& draw_attr)
{
	CHECK_WITH_LOG(m_current_graphics_pso == nullptr, "GLES3: Draw called without pipeline");

	// Apply vertex attributes from PSO layout using the bound VBOs.
	// GLES 3.0: use glVertexAttribPointer (compatibility path; glBindVertexBuffer
	// is ES 3.1+ only).
	CONST auto& layout = m_current_graphics_pso->GetPSODesc().vertex_input_layout;
	if (!layout.empty() && m_bound_vbs[0].buf)
	{
		// Bind the VBO for slot 0
		glBindBuffer(GL_ARRAY_BUFFER, m_bound_vbs[0].buf);

		for (UInt32 i = 0; i < layout.size(); ++i)
		{
			CONST auto& elem = layout[i];
			GLint size = 4;
			GLenum type = GL_FLOAT;
			GLboolean normalized = GL_FALSE;

			switch (elem.attribute_format)
			{
			case ENUM_TEXTURE_FORMAT::R32F:    size = 1; type = GL_FLOAT; break;
			case ENUM_TEXTURE_FORMAT::RG32F:   size = 2; type = GL_FLOAT; break;
			case ENUM_TEXTURE_FORMAT::RGBA32F: size = 4; type = GL_FLOAT; break;
			case ENUM_TEXTURE_FORMAT::RGBA8:   size = 4; type = GL_UNSIGNED_BYTE; normalized = GL_TRUE; break;
			default: break;
			}

			glEnableVertexAttribArray((GLuint)elem.location);
			glVertexAttribPointer((GLuint)elem.location, size, type, normalized,
				(GLsizei)m_bound_vbs[elem.binding].stride,
				(const void*)(uintptr_t)(elem.offset + m_bound_vbs[elem.binding].offset));
		}
	}

	GLenum topology = TranslatePrimitiveTopology(m_current_graphics_pso->GetPrimitiveTopology());
	glDrawArrays(topology, (GLint)draw_attr.firstVertex, (GLsizei)draw_attr.vertexCount);

	// Cleanup vertex attrib arrays
	for (UInt32 i = 0; i < layout.size(); ++i)
		glDisableVertexAttribArray((GLuint)layout[i].location);
}

void GLES3_CommandBuffer::Dispatch(UInt32 groupX, UInt32 groupY, UInt32 groupZ)
{
	(void)groupX; (void)groupY; (void)groupZ;
}

#pragma endregion

#pragma region VERTEX_INDEX

void GLES3_CommandBuffer::SetVertexBuffer(Buffer* buffer, UInt32 slot, UInt32 stride, UInt32 offset)
{
	auto* gl_buf = static_cast<GLES3_Buffer*>(buffer);
	if (!gl_buf || !gl_buf->GetGLBuffer()) return;

	if (slot < 4)
	{
		m_bound_vbs[slot].buf = gl_buf->GetGLBuffer();
		m_bound_vbs[slot].stride = stride;
		m_bound_vbs[slot].offset = offset;
	}
}

void GLES3_CommandBuffer::SetIndexBuffer(Buffer* buffer, UInt32 offset, Bool index32)
{
	auto* gl_buf = static_cast<GLES3_Buffer*>(buffer);
	if (!gl_buf || !gl_buf->GetGLBuffer()) return;

	// Bind IBO to GL_ELEMENT_ARRAY_BUFFER — this binding is captured by the
	// current VAO (glBindVertexArray must be called before this, or we need
	// to store the IBO and bind it at draw time).
	m_current_ibo = gl_buf->GetGLBuffer();
	m_current_index_type = index32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
	m_current_index_offset = offset;
}

void GLES3_CommandBuffer::DrawIndexed(UInt32 indexCount, UInt32 instanceCount,
                                       UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance)
{
	CHECK_WITH_LOG(m_current_graphics_pso == nullptr, "GLES3: DrawIndexed called without pipeline");

	// Apply vertex attributes using the PSO layout (GLES 3.0 compatibility path)
	CONST auto& layout = m_current_graphics_pso->GetPSODesc().vertex_input_layout;
	if (!layout.empty() && m_bound_vbs[0].buf)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_bound_vbs[0].buf);
		for (UInt32 i = 0; i < layout.size(); ++i)
		{
			CONST auto& elem = layout[i];
			GLint size = 4; GLenum type = GL_FLOAT; GLboolean norm = GL_FALSE;
			switch (elem.attribute_format)
			{
			case ENUM_TEXTURE_FORMAT::R32F:    size=1; type=GL_FLOAT; break;
			case ENUM_TEXTURE_FORMAT::RG32F:   size=2; type=GL_FLOAT; break;
			case ENUM_TEXTURE_FORMAT::RGBA32F: size=4; type=GL_FLOAT; break;
			case ENUM_TEXTURE_FORMAT::RGBA8:   size=4; type=GL_UNSIGNED_BYTE; norm=GL_TRUE; break;
			default: break;
			}
			glEnableVertexAttribArray((GLuint)elem.location);
			glVertexAttribPointer((GLuint)elem.location, size, type, norm,
				(GLsizei)m_bound_vbs[elem.binding].stride,
				(const void*)(uintptr_t)(elem.offset + m_bound_vbs[elem.binding].offset));
		}
	}

	// Bind index buffer
	if (m_current_ibo)
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_current_ibo);

	GLenum topology = TranslatePrimitiveTopology(m_current_graphics_pso->GetPrimitiveTopology());
	size_t idx_offset = (size_t)firstIndex * (m_current_index_type == GL_UNSIGNED_INT ? 4 : 2);

	if (instanceCount > 1)
	{
		glDrawElementsInstanced(topology, (GLsizei)indexCount, m_current_index_type,
			(const void*)idx_offset, (GLsizei)instanceCount);
	}
	else
	{
		glDrawElements(topology, (GLsizei)indexCount, m_current_index_type,
			(const void*)idx_offset);
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
