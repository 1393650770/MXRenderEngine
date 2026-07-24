#pragma once
#ifndef _GLES3_TEXTURE_
#define _GLES3_TEXTURE_

#if PLATFORM_GLES3

#include "RHI/RenderTexture.h"
#include <GLES3/gl3.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(GLES3_Texture, public Texture)
#pragma region METHOD
public:
	GLES3_Texture(CONST TextureDesc& desc);
	VIRTUAL ~GLES3_Texture() OVERRIDE;

	// For swapchain backbuffer: wraps the default framebuffer (glBindFramebuffer(0))
	void METHOD(SetAsDefaultFramebuffer)() { m_is_default_fb = true; }
	Bool METHOD(IsDefaultFramebuffer)() CONST { return m_is_default_fb; }

	// For owned textures: wraps an existing GL texture
	void METHOD(SetGLTexture)(GLuint texture);
	GLuint METHOD(GetGLTexture)() CONST { return m_gl_texture; }

	// GL framebuffer for render-to-texture (Phase 1+)
	void METHOD(SetGLFramebuffer)(GLuint fbo) { m_gl_fbo = fbo; }
	GLuint METHOD(GetGLFramebuffer)() CONST { return m_gl_fbo; }
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	GLuint m_gl_texture = 0;
	GLuint m_gl_fbo = 0;
	Bool m_is_default_fb = false;  // swapchain: bind to GL_DRAW_FRAMEBUFFER 0
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _GLES3_TEXTURE_
