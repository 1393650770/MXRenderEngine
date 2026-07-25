#include"RenderTexture.h"
#include<iostream>
#include <algorithm>
using std::max;
using std::min;
#ifdef _MSC_VER
#include <xutility>
#endif
MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
TextureDesc::TextureDesc(const TextureDesc& other)
{
    *this= other;
}

Texture::Texture(const TextureDesc& in_texture_desc):texture_desc(in_texture_desc)
{
    
}

TextureDesc Texture::GetTextureDesc() CONST
{
    return texture_desc;
}

void Texture::SetResourceState(CONST ENUM_RESOURCE_STATE& in_state)
{
	texture_desc.resource_state = in_state;
}

void Texture::UpdateTextureData(CONST TextureDataPayload& texture_data_payload)
{

}


CONST Texture::TextureFormatAttribs& Texture::GetTextureFormatAttribs(ENUM_TEXTURE_FORMAT format)
{
	MYRENDERER_BEGIN_STRUCT(TextureFormatAttribsMap)
	public:
		TextureFormatAttribsMap()
		{
	#define INIT_TEX_FORMAT_INFO(TexFmt, ComponentSize, NumComponents, ComponentType, IsTypeless, BlockWidth, BlockHeight) \
					format_attribs_map[ (UInt32)TexFmt ] = TextureFormatAttribs{ TexFmt,ComponentType, NumComponents, ComponentSize , BlockWidth, BlockHeight, IsTypeless};
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RGBA32F, 4, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Float, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RGB32F, 4, 3, ENUM_TEXTURE_COMPONENT_FORMAT::Float, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RGBA16F, 2, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Float, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RG32F, 4, 2, ENUM_TEXTURE_COMPONENT_FORMAT::Float, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::R32F, 4, 1, ENUM_TEXTURE_COMPONENT_FORMAT::Float, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RGBA32U, 4, 4, ENUM_TEXTURE_COMPONENT_FORMAT::UInt, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RGBA16U, 2, 4, ENUM_TEXTURE_COMPONENT_FORMAT::UInt, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::R32U, 4, 1, ENUM_TEXTURE_COMPONENT_FORMAT::UInt, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::D32, 4, 1, ENUM_TEXTURE_COMPONENT_FORMAT::Depth, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::D32FS8, 4, 2, ENUM_TEXTURE_COMPONENT_FORMAT::DepthStencil, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::BGRA8, 1, 4, ENUM_TEXTURE_COMPONENT_FORMAT::UNorm, false,1,1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RGBA8, 1, 4, ENUM_TEXTURE_COMPONENT_FORMAT::UNorm, false,1,1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RGBA16, 2, 4, ENUM_TEXTURE_COMPONENT_FORMAT::UNorm, false,1,1)

			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::BC3, 16, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::BC1, 8, 3, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::BC2, 16, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::BC4, 8, 1, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::BC5, 16, 2, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::BC6H, 16, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::BC7, 16, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)

			// ETC2 (GLES 3.0 required)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::ETC1, 8, 3, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::ETC2, 8, 3, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::ETC2A, 16, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::ETC2A1, 8, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)

			// ASTC
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::ASTC4x4, 16, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 4, 4)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::ASTC5x5, 16, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 5, 5)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::ASTC6x6, 16, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 6, 6)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::ASTC8x5, 16, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 8, 5)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::ASTC8x6, 16, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 8, 6)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::ASTC10x5, 16, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Compressed, false, 10, 5)

			// Common uncompressed (frequently used)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::R8, 1, 1, ENUM_TEXTURE_COMPONENT_FORMAT::UNorm, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RG8, 1, 2, ENUM_TEXTURE_COMPONENT_FORMAT::UNorm, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::R8U, 1, 1, ENUM_TEXTURE_COMPONENT_FORMAT::UInt, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::R16F, 2, 1, ENUM_TEXTURE_COMPONENT_FORMAT::Float, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RG16F, 2, 2, ENUM_TEXTURE_COMPONENT_FORMAT::Float, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::R32I, 4, 1, ENUM_TEXTURE_COMPONENT_FORMAT::SInt, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RG32I, 4, 2, ENUM_TEXTURE_COMPONENT_FORMAT::SInt, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RG32U, 4, 2, ENUM_TEXTURE_COMPONENT_FORMAT::UInt, false, 1, 1)

			// Depth/Stencil
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::D24S8, 4, 2, ENUM_TEXTURE_COMPONENT_FORMAT::DepthStencil, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::D16, 2, 1, ENUM_TEXTURE_COMPONENT_FORMAT::Depth, false, 1, 1)

			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RGBA16S, 2, 4, ENUM_TEXTURE_COMPONENT_FORMAT::Float, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RGBA8U, 1, 4, ENUM_TEXTURE_COMPONENT_FORMAT::UInt, false, 1, 1)
			INIT_TEX_FORMAT_INFO(ENUM_TEXTURE_FORMAT::RGBA8I, 1, 4, ENUM_TEXTURE_COMPONENT_FORMAT::SInt, false, 1, 1)


		}
	CONST TextureFormatAttribs& operator[](ENUM_TEXTURE_FORMAT format) CONST
		{
			if (format >= ENUM_TEXTURE_FORMAT::None && format < ENUM_TEXTURE_FORMAT::Count)
			{
				const auto& Attribs = format_attribs_map[(UInt32)format];
				CHECK_WITH_LOG(Attribs.component_format == ENUM_TEXTURE_COMPONENT_FORMAT::None, "Texture format attribs map is not initialized correctly")
				return Attribs;
			}
			else
			{
				String error_msg = "Texture format (" + std::to_string((UInt32)format) + ") is out of allowed range [0, " + std::to_string((UInt32)ENUM_TEXTURE_FORMAT::Count - 1) + "]";
				CHECK_WITH_LOG(true, error_msg.c_str());
				return format_attribs_map[0];
			}
		}
	private:
		Array<TextureFormatAttribs, (UInt32)ENUM_TEXTURE_FORMAT::Count> format_attribs_map{};
	MYRENDERER_END_STRUCT

	static CONST TextureFormatAttribsMap s_texture_format_attribs_map;

	return s_texture_format_attribs_map[format];
}


TextureDataPayload::TextureDataPayload(CONST TextureDataPayload& other)
{
    *this = other;
}


TextureDataPayload::MipLevelProperties TextureDataPayload::GetMipLevelProperties(UInt8 in_mip_level) CONST
{
	MipLevelProperties mip_props;
	const auto& format_attribs = Texture::GetTextureFormatAttribs(format);

	mip_props.logical_width = max(width >> in_mip_level, 1u);
	mip_props.logical_height = max(height >> in_mip_level, 1u);
	mip_props.depth = max(depth >> in_mip_level, 1u);
	//TODO: support compressed formats
	if (format_attribs.component_format == ENUM_TEXTURE_COMPONENT_FORMAT::Compressed)
	{
		mip_props.storage_height = Align(mip_props.logical_height, UInt32{ format_attribs.block_height });
		mip_props.storage_width = Align(mip_props.logical_width, UInt32{ format_attribs.block_width });
		mip_props.row_size = (UInt64{ mip_props.storage_width } / UInt32{ format_attribs.block_width }) * UInt32{ format_attribs.single_component_byte_size };
		mip_props.slice_size = mip_props.storage_height/ UInt32{ format_attribs.block_height } * mip_props.row_size;
		mip_props.mip_size = mip_props.slice_size * mip_props.depth;
	}
	else
	{
		mip_props.storage_height = mip_props.logical_height;
		mip_props.storage_width = mip_props.logical_width;
		mip_props.row_size = UInt64{ mip_props.storage_width } *UInt32{ format_attribs.component_count } *UInt32{ format_attribs.single_component_byte_size };
		mip_props.slice_size = mip_props.row_size * mip_props.storage_height;
		mip_props.mip_size = mip_props.slice_size * mip_props.depth;
	}

	return mip_props;
}

MYRENDERER_END_NAMESPACE

MYRENDERER_END_NAMESPACE
