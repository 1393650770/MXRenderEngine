#pragma once

#ifndef _RENDER_TEXTURE_
#define _RENDER_TEXTURE_
#include "RenderResource.h"
#include <vector>
#include <string>


MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)


MYRENDERER_BEGIN_CLASS_WITH_DERIVE(Texture,public RenderResource)
#pragma region METHOD
public:
	Texture(CONST TextureDesc& in_texture_desc );
	VIRTUAL ~Texture() MYDEFAULT;

	VIRTUAL TextureDesc METHOD(GetTextureDesc)() CONST ; 

	VIRTUAL void METHOD(UpdateTextureData)(CONST TextureDataPayload& texture_data_payload);

	MYRENDERER_BEGIN_STRUCT(TextureFormatAttribs)
		ENUM_TEXTURE_FORMAT format = ENUM_TEXTURE_FORMAT::None;
		ENUM_TEXTURE_COMPONENT_FORMAT component_format = ENUM_TEXTURE_COMPONENT_FORMAT::None;
		UInt8 component_count = 0;
		UInt8 single_component_byte_size = 0;
		UInt8 block_width = 0;
		UInt8 block_height = 0;
		Bool  is_typeless = false;
	MYRENDERER_END_STRUCT

	static CONST TextureFormatAttribs& METHOD(GetTextureFormatAttribs)(ENUM_TEXTURE_FORMAT format);

	void METHOD(SetResourceState)(CONST ENUM_RESOURCE_STATE& state);
protected:
	
private:

#pragma endregion


#pragma region MEMBER
public:

protected:
	TextureDesc texture_desc;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif