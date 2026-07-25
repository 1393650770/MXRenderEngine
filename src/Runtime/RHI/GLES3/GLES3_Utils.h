#pragma once
#ifndef _GLES3_UTILS_
#define _GLES3_UTILS_

#if PLATFORM_GLES3

#include <GLES3/gl3.h>
#include <GLES3/gl2ext.h>
#include "RHI/RenderEnum.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

// GLES3 Utils: Engine enum -> GL enum translations.

inline GLenum TranslateTextureFormat_Internal(ENUM_TEXTURE_FORMAT fmt)
{
	switch (fmt)
	{
	case ENUM_TEXTURE_FORMAT::RGBA8:   return GL_RGBA8;
	case ENUM_TEXTURE_FORMAT::BGRA8:   // BGRA8: WebGL 2.0 might not expose BGRA8 internal format
		return GL_RGBA8;  // fallback: BGRA pixels uploaded as RGBA
	case ENUM_TEXTURE_FORMAT::RGBA16F: return GL_RGBA16F;
	case ENUM_TEXTURE_FORMAT::R8:      return GL_R8;
	case ENUM_TEXTURE_FORMAT::R32U:    return GL_R32UI;
	case ENUM_TEXTURE_FORMAT::R32F:    return GL_R32F;
	case ENUM_TEXTURE_FORMAT::RG32F:   return GL_RG32F;
	case ENUM_TEXTURE_FORMAT::RGBA32F: return GL_RGBA32F;
	case ENUM_TEXTURE_FORMAT::D32:     return GL_DEPTH_COMPONENT32F;
	case ENUM_TEXTURE_FORMAT::D16:     return GL_DEPTH_COMPONENT16;
	case ENUM_TEXTURE_FORMAT::D24S8:   return GL_DEPTH24_STENCIL8;
	case ENUM_TEXTURE_FORMAT::D32FS8:  return GL_DEPTH32F_STENCIL8;
	default:                            return GL_RGBA8;
	}
}

inline GLenum TranslateTextureFormat_Pixel(ENUM_TEXTURE_FORMAT fmt)
{
	switch (fmt)
	{
	case ENUM_TEXTURE_FORMAT::RGBA8:   return GL_RGBA;
	case ENUM_TEXTURE_FORMAT::BGRA8:   return GL_RGBA;  // fallback
	case ENUM_TEXTURE_FORMAT::RGBA16F: return GL_RGBA;
	case ENUM_TEXTURE_FORMAT::R8:      return GL_RED;
	case ENUM_TEXTURE_FORMAT::R32U:    return GL_RED_INTEGER;
	case ENUM_TEXTURE_FORMAT::R32F:    return GL_RED;
	case ENUM_TEXTURE_FORMAT::RG32F:   return GL_RG;
	case ENUM_TEXTURE_FORMAT::RGBA32F: return GL_RGBA;
	case ENUM_TEXTURE_FORMAT::D32:     return GL_DEPTH_COMPONENT;
	case ENUM_TEXTURE_FORMAT::D16:     return GL_DEPTH_COMPONENT;
	case ENUM_TEXTURE_FORMAT::D24S8:   return GL_DEPTH_STENCIL;
	case ENUM_TEXTURE_FORMAT::D32FS8:  return GL_DEPTH_STENCIL;
	default:                            return GL_RGBA;
	}
}

inline GLenum TranslateTextureFormat_Type(ENUM_TEXTURE_FORMAT fmt)
{
	switch (fmt)
	{
	case ENUM_TEXTURE_FORMAT::RGBA8:   return GL_UNSIGNED_BYTE;
	case ENUM_TEXTURE_FORMAT::BGRA8:   return GL_UNSIGNED_BYTE;
	case ENUM_TEXTURE_FORMAT::RGBA16F: return GL_HALF_FLOAT;
	case ENUM_TEXTURE_FORMAT::R8:      return GL_UNSIGNED_BYTE;
	case ENUM_TEXTURE_FORMAT::R32U:    return GL_UNSIGNED_INT;
	case ENUM_TEXTURE_FORMAT::R32F:    return GL_FLOAT;
	case ENUM_TEXTURE_FORMAT::RG32F:   return GL_FLOAT;
	case ENUM_TEXTURE_FORMAT::RGBA32F: return GL_FLOAT;
	case ENUM_TEXTURE_FORMAT::D32:     return GL_FLOAT;
	case ENUM_TEXTURE_FORMAT::D16:     return GL_UNSIGNED_SHORT;
	case ENUM_TEXTURE_FORMAT::D24S8:   return GL_UNSIGNED_INT_24_8;
	case ENUM_TEXTURE_FORMAT::D32FS8:  return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
	default:                            return GL_UNSIGNED_BYTE;
	}
}

inline GLenum TranslatePrimitiveTopology(ENUM_PRIMITIVE_TYPE topo)
{
	switch (topo)
	{
	case ENUM_PRIMITIVE_TYPE::TriangleList:  return GL_TRIANGLES;
	case ENUM_PRIMITIVE_TYPE::TriangleStrip: return GL_TRIANGLE_STRIP;
	case ENUM_PRIMITIVE_TYPE::TriangleFan:   return GL_TRIANGLE_FAN;
	case ENUM_PRIMITIVE_TYPE::LineList:      return GL_LINES;
	case ENUM_PRIMITIVE_TYPE::PointList:     return GL_POINTS;
	default:                                  return GL_TRIANGLES;
	}
}

inline GLenum TranslateBufferType_ToTarget(ENUM_BUFFER_TYPE type)
{
	if (EnumHasAnyFlags(type, ENUM_BUFFER_TYPE::Index))  return GL_ELEMENT_ARRAY_BUFFER;
	if (EnumHasAnyFlags(type, ENUM_BUFFER_TYPE::Vertex)) return GL_ARRAY_BUFFER;
	if (EnumHasAnyFlags(type, ENUM_BUFFER_TYPE::Uniform)) return GL_UNIFORM_BUFFER;
	// GLES 3.0: no SSBO; Storage maps to Uniform in shaders (std140)
	if (EnumHasAnyFlags(type, ENUM_BUFFER_TYPE::Storage)) return GL_UNIFORM_BUFFER;
	return GL_ARRAY_BUFFER;
}

inline GLenum TranslateBufferType_ToUsage(ENUM_BUFFER_TYPE type)
{
	if (EnumHasAnyFlags(type, ENUM_BUFFER_TYPE::Dynamic)) return GL_DYNAMIC_DRAW;
	if (EnumHasAnyFlags(type, ENUM_BUFFER_TYPE::Staging)) return GL_STREAM_DRAW;
	return GL_STATIC_DRAW;
}

// ENUM_STENCIL_FUNCTION doubles as depth compare function in this RHI
inline GLenum TranslateDepthFunction(ENUM_STENCIL_FUNCTION func)
{
	switch (func)
	{
	case ENUM_STENCIL_FUNCTION::ENUM_NEVER:        return GL_NEVER;
	case ENUM_STENCIL_FUNCTION::ENUM_LESS:         return GL_LESS;
	case ENUM_STENCIL_FUNCTION::ENUM_EQUAL:        return GL_EQUAL;
	case ENUM_STENCIL_FUNCTION::ENUM_LEQUAL:       return GL_LEQUAL;
	case ENUM_STENCIL_FUNCTION::ENUM_GREATER:      return GL_GREATER;
	case ENUM_STENCIL_FUNCTION::ENUM_NOTEQUAL:     return GL_NOTEQUAL;
	case ENUM_STENCIL_FUNCTION::ENUM_NOT_EQUAL:    return GL_NOTEQUAL;
	case ENUM_STENCIL_FUNCTION::ENUM_LESSOREQUAL:  return GL_LEQUAL;
	case ENUM_STENCIL_FUNCTION::ENUM_GREATEROREQUAL: return GL_GEQUAL;
	case ENUM_STENCIL_FUNCTION::ENUM_ALWAYS:       return GL_ALWAYS;
	default:                                        return GL_ALWAYS;
	}
}

inline GLenum TranslateCullMode(ENUM_RASTER_CULLMODE mode)
{
	switch (mode)
	{
	case ENUM_RASTER_CULLMODE::Back:  return GL_BACK;
	case ENUM_RASTER_CULLMODE::Front: return GL_FRONT;
	case ENUM_RASTER_CULLMODE::None:  return 0;  // caller: glDisable(GL_CULL_FACE)
	default:                           return GL_BACK;
	}
}

// Note: GLES 3.0 does not support glPolygonMode; only GL_FILL is available.
// TranslateRasterFillMode is not used by Phase 0 HelloTriangle.

inline GLenum TranslateBlendFactor(ENUM_BLEND_FACTOR factor)
{
	switch (factor)
	{
	case ENUM_BLEND_FACTOR::ENUM_ZERO:                  return GL_ZERO;
	case ENUM_BLEND_FACTOR::ENUM_ONE:                   return GL_ONE;
	case ENUM_BLEND_FACTOR::ENUM_SRC_COLOR:             return GL_SRC_COLOR;
	case ENUM_BLEND_FACTOR::ENUM_ONE_MINUS_SRC_COLOR:   return GL_ONE_MINUS_SRC_COLOR;
	case ENUM_BLEND_FACTOR::ENUM_SRC_ALPHA:             return GL_SRC_ALPHA;
	case ENUM_BLEND_FACTOR::ENUM_ONE_MINUS_SRC_ALPHA:   return GL_ONE_MINUS_SRC_ALPHA;
	case ENUM_BLEND_FACTOR::ENUM_DST_ALPHA:             return GL_DST_ALPHA;
	case ENUM_BLEND_FACTOR::ENUM_ONE_MINUS_DST_ALPHA:   return GL_ONE_MINUS_DST_ALPHA;
	case ENUM_BLEND_FACTOR::ENUM_DST_COLOR:             return GL_DST_COLOR;
	case ENUM_BLEND_FACTOR::ENUM_ONE_MINUS_DST_COLOR:   return GL_ONE_MINUS_DST_COLOR;
	case ENUM_BLEND_FACTOR::EUNUM_SRC_ALPHA_SATURATE:   return GL_SRC_ALPHA_SATURATE;
	default:                                             return GL_ONE;
	}
}

inline GLenum TranslateBlendEquation(ENUM_BLEND_EQUATION eq)
{
	switch (eq)
	{
	case ENUM_BLEND_EQUATION::ENUM_ADD:         return GL_FUNC_ADD;
	case ENUM_BLEND_EQUATION::ENUM_SUB:          return GL_FUNC_SUBTRACT;
	case ENUM_BLEND_EQUATION::ENUM_REVERSE_SUB:  return GL_FUNC_REVERSE_SUBTRACT;
	case ENUM_BLEND_EQUATION::ENUM_MIN:          return GL_MIN;
	case ENUM_BLEND_EQUATION::ENUM_MAX:          return GL_MAX;
	default:                                      return GL_FUNC_ADD;
	}
}

// ---- Compressed Texture Format Translation ----
// GLES 3.0 core requires ETC2/EAC support. ASTC is via extension.

inline GLenum TranslateCompressedFormat_Internal(ENUM_TEXTURE_FORMAT fmt)
{
	switch (fmt)
	{
	// ETC2 (GLES 3.0 core)
	case ENUM_TEXTURE_FORMAT::ETC1:  return GL_ETC1_RGB8_OES;
	case ENUM_TEXTURE_FORMAT::ETC2:  return GL_COMPRESSED_RGB8_ETC2;
	case ENUM_TEXTURE_FORMAT::ETC2A: return GL_COMPRESSED_RGBA8_ETC2_EAC;
	case ENUM_TEXTURE_FORMAT::ETC2A1:return GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
	// ASTC (extension: WEBGL_compressed_texture_astc)
	case ENUM_TEXTURE_FORMAT::ASTC4x4:  return 0x93B0; // GL_COMPRESSED_RGBA_ASTC_4x4_KHR
	case ENUM_TEXTURE_FORMAT::ASTC5x5:  return 0x93B1;
	case ENUM_TEXTURE_FORMAT::ASTC6x6:  return 0x93B2;
	case ENUM_TEXTURE_FORMAT::ASTC8x5:  return 0x93B3;
	case ENUM_TEXTURE_FORMAT::ASTC8x6:  return 0x93B4;
	case ENUM_TEXTURE_FORMAT::ASTC10x5: return 0x93B5;
	default: return 0;
	}
}

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _GLES3_UTILS_
