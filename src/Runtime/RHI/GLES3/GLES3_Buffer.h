#pragma once
#ifndef _GLES3_BUFFER_
#define _GLES3_BUFFER_

#if PLATFORM_GLES3

#include "RHI/RenderBuffer.h"
#include <GLES3/gl3.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(GLES3_Buffer, public Buffer)
#pragma region METHOD
public:
	GLES3_Buffer(CONST BufferDesc& desc);
	VIRTUAL ~GLES3_Buffer() OVERRIDE;

	VIRTUAL void* METHOD(Map)(CONST ENUM_MAP_TYPE& map_type, CONST ENUM_MAP_FLAG& map_flag) OVERRIDE FINAL;
	VIRTUAL void METHOD(Unmap)() OVERRIDE FINAL;

	void METHOD(SetGLBuffer)(GLuint buffer, void* mapped_ptr = nullptr);
	GLuint METHOD(GetGLBuffer)() CONST { return m_gl_buffer; }
	GLenum METHOD(GetGLTarget)() CONST { return m_gl_target; }
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	GLuint m_gl_buffer = 0;
	GLenum m_gl_target = GL_ARRAY_BUFFER;
	void* m_mapped_ptr = nullptr;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _GLES3_BUFFER_
