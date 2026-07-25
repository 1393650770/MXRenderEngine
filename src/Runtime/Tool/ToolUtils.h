#pragma once
#ifndef _TOOLUTILS_
#define _TOOLUTILS_
#include "Core/ConstDefine.h"
#include "RHI/RenderEnum.h"
#include "Render/Core/RenderGraphDefinition.h"
#if !PLATFORM_GLES3 && !PLATFORM_WGPU
#include "gli/format.hpp"
#endif

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Tool)

// Enum ↔ string conversion utilities
CONST Char* EnumToString(ENUM_TEXTURE_FORMAT format);
ENUM_TEXTURE_FORMAT StringToEnum_TextureFormat(CONST String& str);
CONST Char* EnumToString(Render::RDGPassKind kind);
Render::RDGPassKind StringToEnum_PassKind(CONST String& str);
CONST Char* EnumToString(Render::RDGResourceKind kind);
Render::RDGResourceKind StringToEnum_ResourceKind(CONST String& str);
CONST Char* EnumToString(ENUM_BUFFER_TYPE type);
CONST Char* EnumToString(ENUM_TEXTURE_TYPE type);
ENUM_TEXTURE_TYPE StringToEnum_TextureType(CONST String& str);
ENUM_BUFFER_TYPE StringToEnum_BufferType(CONST String& str);

MYRENDERER_BEGIN_CLASS( ToolUtils )
public:
#if !PLATFORM_GLES3 && !PLATFORM_WGPU
	static ENUM_TEXTURE_FORMAT METHOD(TranslateGliFormatToEngineFormat)(CONST gli::format& in_format);
#endif

MYRENDERER_END_CLASS
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif