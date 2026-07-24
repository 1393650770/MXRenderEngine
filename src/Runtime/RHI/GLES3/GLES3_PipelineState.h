#pragma once
#ifndef _GLES3_PIPELINESTATE_
#define _GLES3_PIPELINESTATE_

#if PLATFORM_GLES3

#include "RHI/RenderPipelineState.h"
#include <GLES3/gl3.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(GLES3_PipelineState, public RenderPipelineState)
#pragma region METHOD
public:
	GLES3_PipelineState() MYDEFAULT;
	GLES3_PipelineState(CONST RenderGraphiPipelineStateDesc& in_desc);
	VIRTUAL ~GLES3_PipelineState() OVERRIDE;

	VIRTUAL void CreateShaderResourceBinding(ShaderResourceBinding*& out_srb, Bool init_static_resource = false) OVERRIDE FINAL;

	void METHOD(SetGLProgram)(GLuint program) { if (m_program) glDeleteProgram(m_program); m_program = program; }
	GLuint METHOD(GetGLProgram)() CONST { return m_program; }

	void METHOD(SetVAO)(GLuint vao) { m_vao = vao; }
	GLuint METHOD(GetVAO)() CONST { return m_vao; }

	// State fields from the PSO desc (applied at SetGraphicsPipeline time)
	void METHOD(ApplyState)(GLboolean depth_write = GL_FALSE) CONST;
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	GLuint m_program = 0;
	GLuint m_vao = 0;  // vertex layout; 0 = hardcoded (HelloTriangle pattern)
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _GLES3_PIPELINESTATE_
