#pragma once
#ifndef _GLES3_UTILS_
#define _GLES3_UTILS_

#if PLATFORM_GLES3

#include <GLES3/gl3.h>
#include "RHI/RenderEnum.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

// GLES3 Utils: Engine enum -> GL enum translations.
// Only formats/topologies needed by HelloTriangle are implemented; expand as needed.

inline GLenum TranslateTextureFormat_Internal(ENUM_TEXTURE_FORMAT fmt)
{
	switch (fmt)
	{
	case ENUM_TEXTURE_FORMAT::RGBA8:   return GL_RGBA8;
	case ENUM_TEXTURE_FORMAT::BGRA8:   return GL_BGRA8_EXT;  // EXT_texture_format_BGRA8888
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
	case ENUM_TEXTURE_FORMAT::BGRA8:   return GL_BGRA_EXT;
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
	case ENUM_PRIMITIVE_TYPE::LineStrip:     return GL_LINE_STRIP;
	case ENUM_PRIMITIVE_TYPE::PointList:     return GL_POINTS;
	default:                                  return GL_TRIANGLES;
	}
}

inline GLenum TranslateBufferType_ToTarget(ENUM_BUFFER_TYPE type)
{
	if (EnumHasFlags(type, ENUM_BUFFER_TYPE::Index))  return GL_ELEMENT_ARRAY_BUFFER;
	if (EnumHasFlags(type, ENUM_BUFFER_TYPE::Vertex)) return GL_ARRAY_BUFFER;
	if (EnumHasFlags(type, ENUM_BUFFER_TYPE::Uniform)) return GL_UNIFORM_BUFFER;
	if (EnumHasFlags(type, ENUM_BUFFER_TYPE::Storage)) return GL_SHADER_STORAGE_BUFFER;
	return GL_ARRAY_BUFFER;
}

inline GLenum TranslateBufferType_ToUsage(ENUM_BUFFER_TYPE type)
{
	if (EnumHasFlags(type, ENUM_BUFFER_TYPE::Dynamic)) return GL_DYNAMIC_DRAW;
	if (EnumHasFlags(type, ENUM_BUFFER_TYPE::Staging)) return GL_STREAM_DRAW;
	return GL_STATIC_DRAW;
}

inline GLenum TranslateDepthFunction(ENUM_DEPTH_FUNCTION func)
{
	switch (func)
	{
	case ENUM_DEPTH_FUNCTION::Never:        return GL_NEVER;
	case ENUM_DEPTH_FUNCTION::Less:         return GL_LESS;
	case ENUM_DEPTH_FUNCTION::Equal:        return GL_EQUAL;
	case ENUM_DEPTH_FUNCTION::LessEqual:    return GL_LEQUAL;
	case ENUM_DEPTH_FUNCTION::Greater:      return GL_GREATER;
	case ENUM_DEPTH_FUNCTION::NotEqual:     return GL_NOTEQUAL;
	case ENUM_DEPTH_FUNCTION::GreaterEqual: return GL_GEQUAL;
	case ENUM_DEPTH_FUNCTION::Always:       return GL_ALWAYS;
	default:                                 return GL_LESS;
	}
}

inline GLenum TranslateCullMode(ENUM_RASTER_CULLMODE mode)
{
	switch (mode)
	{
	case ENUM_RASTER_CULLMODE::Back:  return GL_BACK;
	case ENUM_RASTER_CULLMODE::Front: return GL_FRONT;
	case ENUM_RASTER_CULLMODE::None:  return 0;  // glDisable(GL_CULL_FACE)
	default:                           return GL_BACK;
	}
}

inline GLenum TranslateRasterFillMode(ENUM_RASTER_FILLMODE mode)
{
	switch (mode)
	{
	case ENUM_RASTER_FILLMODE::Solid:     return GL_FILL;
	case ENUM_RASTER_FILLMODE::Wireframe: return GL_LINE;
	default:                               return GL_FILL;
	}
}

inline GLenum TranslateBlendFactor(ENUM_BLEND_FACTOR factor)
{
	switch (factor)
	{
	case ENUM_BLEND_FACTOR::Zero:             return GL_ZERO;
	case ENUM_BLEND_FACTOR::One:              return GL_ONE;
	case ENUM_BLEND_FACTOR::SrcColor:         return GL_SRC_COLOR;
	case ENUM_BLEND_FACTOR::InvSrcColor:      return GL_ONE_MINUS_SRC_COLOR;
	case ENUM_BLEND_FACTOR::SrcAlpha:         return GL_SRC_ALPHA;
	case ENUM_BLEND_FACTOR::InvSrcAlpha:      return GL_ONE_MINUS_SRC_ALPHA;
	case ENUM_BLEND_FACTOR::DstAlpha:         return GL_DST_ALPHA;
	case ENUM_BLEND_FACTOR::InvDstAlpha:      return GL_ONE_MINUS_DST_ALPHA;
	case ENUM_BLEND_FACTOR::DstColor:         return GL_DST_COLOR;
	case ENUM_BLEND_FACTOR::InvDstColor:      return GL_ONE_MINUS_DST_COLOR;
	case ENUM_BLEND_FACTOR::SrcAlphaSaturate: return GL_SRC_ALPHA_SATURATE;
	default:                                   return GL_ONE;
	}
}

inline GLenum TranslateBlendEquation(ENUM_BLEND_EQUATION eq)
{
	switch (eq)
	{
	case ENUM_BLEND_EQUATION::Add:             return GL_FUNC_ADD;
	case ENUM_BLEND_EQUATION::Subtract:        return GL_FUNC_SUBTRACT;
	case ENUM_BLEND_EQUATION::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
	case ENUM_BLEND_EQUATION::Min:             return GL_MIN;
	case ENUM_BLEND_EQUATION::Max:             return GL_MAX;
	default:                                    return GL_FUNC_ADD;
	}
}

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _GLES3_UTILS_
