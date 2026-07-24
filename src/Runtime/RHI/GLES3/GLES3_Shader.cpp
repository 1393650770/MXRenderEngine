#if PLATFORM_GLES3

#include "RHI/GLES3/GLES3_Shader.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

GLES3_Shader::GLES3_Shader(CONST ShaderDesc& desc)
	: Shader(desc)
{
}

GLES3_Shader::GLES3_Shader(CONST ShaderDesc& desc, CONST ShaderDataPayload& data)
	: Shader(desc, data)
{
}

GLES3_Shader::~GLES3_Shader()
{
	if (m_shader_obj)
	{
		glDeleteShader(m_shader_obj);
		m_shader_obj = 0;
	}
}

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
