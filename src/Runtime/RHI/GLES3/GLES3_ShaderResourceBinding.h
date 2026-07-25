#pragma once
#ifndef _GLES3_SRB_
#define _GLES3_SRB_

#if PLATFORM_GLES3

#include "RHI/RenderShader.h"
#include <GLES3/gl3.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

class GLES3_Texture;
class GLES3_Buffer;

// GLES3 ShaderResourceBinding: maps engine resource names to GL bindings.
// Phase 1: supports SampledTexture + UniformBuffer bindings.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(GLES3_ShaderResourceBinding, public ShaderResourceBinding)
#pragma region METHOD
public:
	GLES3_ShaderResourceBinding() MYDEFAULT;
	VIRTUAL ~GLES3_ShaderResourceBinding() OVERRIDE;

	VIRTUAL void METHOD(SetResource)(CONST String& name, CONST RenderResource* resource) OVERRIDE FINAL;
	VIRTUAL void METHOD(FlushDescriptorWrites)() OVERRIDE FINAL;

	// Apply all bindings using the currently active GL program
	void METHOD(ApplyBindings)(GLuint current_program);
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	struct TextureBinding
	{
		String name;
		const GLES3_Texture* texture = nullptr;
		GLuint tex_unit = 0;
	};

	struct BufferBinding
	{
		String name;
		const GLES3_Buffer* buffer = nullptr;
		GLint location = -1;
	};

	Vector<TextureBinding> m_textures;
	Vector<BufferBinding> m_buffers;
	UInt32 m_next_tex_unit = 0;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _GLES3_SRB_
