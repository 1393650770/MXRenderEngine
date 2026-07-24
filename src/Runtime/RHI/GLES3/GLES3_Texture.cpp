#if PLATFORM_GLES3

#include "RHI/GLES3/GLES3_Texture.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

GLES3_Texture::GLES3_Texture(CONST TextureDesc& desc)
	: Texture(desc)
{
}

GLES3_Texture::~GLES3_Texture()
{
	if (m_gl_fbo)
	{
		glDeleteFramebuffers(1, &m_gl_fbo);
		m_gl_fbo = 0;
	}
	if (!m_is_default_fb && m_gl_texture)
	{
		glDeleteTextures(1, &m_gl_texture);
		m_gl_texture = 0;
	}
}

void GLES3_Texture::SetGLTexture(GLuint texture)
{
	if (!m_is_default_fb && m_gl_texture) glDeleteTextures(1, &m_gl_texture);
	m_gl_texture = texture;
}

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
