#pragma once
#ifndef _GLES3_SRB_
#define _GLES3_SRB_

#if PLATFORM_GLES3

#include "RHI/RenderShader.h"
#include <GLES3/gl3.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

// Minimal SRB for HelloTriangle (no resources bound).
// Phase C/future: implement uniform/texture bindings.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(GLES3_ShaderResourceBinding, public ShaderResourceBinding)
#pragma region METHOD
public:
	GLES3_ShaderResourceBinding() MYDEFAULT;
	VIRTUAL ~GLES3_ShaderResourceBinding() OVERRIDE;

	VIRTUAL void METHOD(SetResource)(CONST String& name, CONST RenderResource* resource) OVERRIDE FINAL;
	VIRTUAL void METHOD(FlushDescriptorWrites)() OVERRIDE FINAL;

	void METHOD(ApplyBindings)();  // Called before Draw to set glUniform*/glBindTexture
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	struct Binding
	{
		String name;
		ENUM_BINDING_RESOURCE_TYPE type = ENUM_BINDING_RESOURCE_TYPE::Invalid;
		union { GLuint texture = 0; GLuint buffer; };
		GLint location = -1;
	};
	Vector<Binding> m_bindings;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _GLES3_SRB_
