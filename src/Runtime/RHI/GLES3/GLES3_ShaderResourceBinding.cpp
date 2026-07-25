#if PLATFORM_GLES3

#include "RHI/GLES3/GLES3_ShaderResourceBinding.h"
#include "RHI/GLES3/GLES3_Texture.h"
#include "RHI/GLES3/GLES3_Buffer.h"
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

GLES3_ShaderResourceBinding::~GLES3_ShaderResourceBinding()
{
}

void GLES3_ShaderResourceBinding::SetResource(CONST String& name, CONST RenderResource* resource)
{
	if (!resource) return;

	// Try texture binding
	if (auto* tex = dynamic_cast<const GLES3_Texture*>(resource))
	{
		TextureBinding tb;
		tb.name = name;
		tb.texture = tex;
		tb.tex_unit = m_next_tex_unit++;
		m_textures.push_back(tb);
		return;
	}

	// Try buffer binding
	if (auto* buf = dynamic_cast<const GLES3_Buffer*>(resource))
	{
		BufferBinding bb;
		bb.name = name;
		bb.buffer = buf;
		bb.location = -1;  // resolved at ApplyBindings time
		m_buffers.push_back(bb);
		return;
	}

	std::cout << "[GLES3 SRB] SetResource: unknown resource type for '" << name << "'" << std::endl;
}

void GLES3_ShaderResourceBinding::FlushDescriptorWrites()
{
	// Deferred writes not used; GL state is set immediately at ApplyBindings
}

void GLES3_ShaderResourceBinding::ApplyBindings(GLuint current_program)
{
	if (!current_program) return;

	// Bind textures
	for (auto& tb : m_textures)
	{
		if (!tb.texture || !tb.texture->GetGLTexture()) continue;

		GLint loc = glGetUniformLocation(current_program, tb.name.c_str());
		if (loc >= 0)
		{
			glActiveTexture(GL_TEXTURE0 + tb.tex_unit);
			glBindTexture(GL_TEXTURE_2D, tb.texture->GetGLTexture());
			glUniform1i(loc, (GLint)tb.tex_unit);
		}
	}

	// Bind uniform buffers
	for (auto& bb : m_buffers)
	{
		if (!bb.buffer || !bb.buffer->GetGLBuffer()) continue;

		// Try to find as a uniform block index first
		GLuint block_idx = glGetUniformBlockIndex(current_program, bb.name.c_str());
		if (block_idx != GL_INVALID_INDEX)
		{
			GLuint bind_point = 0;
			glUniformBlockBinding(current_program, block_idx, bind_point);
			glBindBufferBase(GL_UNIFORM_BUFFER, bind_point, bb.buffer->GetGLBuffer());
			continue;
		}

		// Try as individual uniform (e.g. uniform mat4, not in a block).
		// Read from client-side buffer copy (WebGL can't glMapBufferRange with
		// GL_MAP_READ_BIT, and glGetBufferSubData doesn't exist in GLES 3.0).
		GLint loc = glGetUniformLocation(current_program, bb.name.c_str());
		if (loc >= 0)
		{
			const auto& buf_desc = bb.buffer->GetBufferDesc();
			// mat4 (64 bytes): upload as glUniformMatrix4fv
			if (buf_desc.size == sizeof(float) * 16)
			{
				const void* data = bb.buffer->GetClientData();
				if (data)
					glUniformMatrix4fv(loc, 1, GL_FALSE, static_cast<const GLfloat*>(data));
			}
		}
	}
}

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
