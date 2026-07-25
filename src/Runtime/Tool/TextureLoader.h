#pragma once
#ifndef _TEXTURELOADER_
#define _TEXTURELOADER_
#include "Core/ConstDefine.h"
#include "RHI/RenderResource.h"
#include <atomic>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
struct TextureDataPayload; 
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Tool)
MYRENDERER_BEGIN_CLASS( TextureLoader)
#pragma region METHOD
public:
	static RHI::TextureDataPayload METHOD(LoadTextureData)(CONST String& filename);
	static void METHOD(LoadTextureData)(CONST String& filename, RHI::TextureDataPayload* data, std::atomic_bool& result);
protected:

private:

#pragma endregion

#pragma region MEMBER
public:

protected:

private:

#pragma endregion


MYRENDERER_END_CLASS


MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif