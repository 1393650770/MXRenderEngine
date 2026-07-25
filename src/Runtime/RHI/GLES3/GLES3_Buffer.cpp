#if PLATFORM_GLES3

#include "RHI/GLES3/GLES3_Buffer.h"
#include "RHI/GLES3/GLES3_Utils.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

GLES3_Buffer::GLES3_Buffer(CONST BufferDesc& desc)
	: Buffer(desc)
{
	m_gl_target = TranslateBufferType_ToTarget(desc.type);
	m_client_data.resize(desc.size, 0);
}

GLES3_Buffer::~GLES3_Buffer()
{
	if (m_gl_buffer)
	{
		glDeleteBuffers(1, &m_gl_buffer);
		m_gl_buffer = 0;
	}
	m_mapped_ptr = nullptr;
}

void* GLES3_Buffer::Map(CONST ENUM_MAP_TYPE& map_type, CONST ENUM_MAP_FLAG& map_flag)
{
	if (!m_gl_buffer) return nullptr;

	// Return client-side pointer (glMapBufferRange is unreliable in WebGL:
	// requires INVALIDATE_* flags, and GL_MAP_READ_BIT is unsupported).
	// Data sync: Unmap() uploads to GL; GetClientData() allows reads.
	(void)map_type;
	(void)map_flag;
	m_mapped_ptr = m_client_data.data();
	return m_mapped_ptr;
}

void GLES3_Buffer::Unmap()
{
	if (m_gl_buffer && m_mapped_ptr)
	{
		// Upload client-side data to GL buffer
		glBindBuffer(m_gl_target, m_gl_buffer);
		glBufferSubData(m_gl_target, 0, buffer_desc.size, m_client_data.data());
		glBindBuffer(m_gl_target, 0);
		m_mapped_ptr = nullptr;
	}
}

void GLES3_Buffer::SetData(const void* data, UInt32 size, UInt32 offset)
{
	if (!m_gl_buffer || !data) return;
	// Also update client-side copy
	if (offset + size <= m_client_data.size())
		memcpy(m_client_data.data() + offset, data, size);
	glBindBuffer(m_gl_target, m_gl_buffer);
	glBufferSubData(m_gl_target, (GLintptr)offset, (GLsizeiptr)size, data);
	glBindBuffer(m_gl_target, 0);
}

void GLES3_Buffer::SetGLBuffer(GLuint buffer, void* mapped_ptr)
{
	if (m_gl_buffer) glDeleteBuffers(1, &m_gl_buffer);
	m_gl_buffer = buffer;
	m_mapped_ptr = mapped_ptr;
}

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
