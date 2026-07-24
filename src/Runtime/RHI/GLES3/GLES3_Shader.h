#pragma once
#ifndef _GLES3_SHADER_
#define _GLES3_SHADER_

#if PLATFORM_GLES3

#include "RHI/RenderShader.h"
#include <GLES3/gl3.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(GLES3_Shader, public Shader)
#pragma region METHOD
public:
	GLES3_Shader(CONST ShaderDesc& desc);
	GLES3_Shader(CONST ShaderDesc& desc, CONST ShaderDataPayload& data);
	VIRTUAL ~GLES3_Shader() OVERRIDE;

	void METHOD(SetShaderObject)(GLuint shader_obj) { m_shader_obj = shader_obj; }
	GLuint METHOD(GetShaderObject)() CONST { return m_shader_obj; }

	// GLSL source string (used in Phase 0; Phase 1+ SPIRV-Cross auto-generated)
	void METHOD(SetGLSLSource)(CONST String& source) { m_glsl_source = source; }
	CONST String& METHOD(GetGLSLSource)() CONST { return m_glsl_source; }
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	GLuint m_shader_obj = 0;
	String m_glsl_source;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _GLES3_SHADER_
