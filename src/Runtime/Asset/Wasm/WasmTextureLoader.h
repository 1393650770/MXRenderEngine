#pragma once
#ifndef _WASM_TEXTURE_LOADER_
#define _WASM_TEXTURE_LOADER_

#if PLATFORM_GLES3

// Wasm Texture Loader — stb_image adapter for browser/WASM.
// Decodes PNG/JPEG/BMP into TextureDataPayload usable by the RHI layer.
// stb_image is already compiled in wasm (src/ThirdParty/stb_image/).

#include "stb_image/stb_image.h"
#include "RHI/RenderResource.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Asset)
MYRENDERER_BEGIN_NAMESPACE(Wasm)

MYRENDERER_BEGIN_CLASS(WasmTextureLoader)
#pragma region METHOD
public:
	// Load from memory buffer (e.g. from AsyncFileIO/fetch).
	// Returns true on success, fills payload.
	static Bool METHOD(LoadFromMemory)(const UInt8* data, size_t size, RHI::TextureDesc& out_desc, Vector<UInt8>& out_pixels)
	{
		int w = 0, h = 0, channels = 0;
		stbi_uc* pixels = stbi_load_from_memory((const stbi_uc*)data, (int)size, &w, &h, &channels, 4); // force RGBA
		if (!pixels) return false;

		out_desc.width = (UInt32)w;
		out_desc.height = (UInt32)h;
		out_desc.format = RHI::ENUM_TEXTURE_FORMAT::RGBA8;
		out_desc.type = RHI::ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
		out_desc.usage = RHI::ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
		out_desc.mip_level = 1;

		size_t pixel_count = (size_t)w * h * 4;
		out_pixels.resize(pixel_count);
		memcpy(out_pixels.data(), pixels, pixel_count);
		stbi_image_free(pixels);
		return true;
	}

	// Quick info without full decode
	static Bool METHOD(GetInfo)(const UInt8* data, size_t size, Int& w, Int& h, Int& channels)
	{
		return stbi_info_from_memory((const stbi_uc*)data, (int)size, &w, &h, &channels) != 0;
	}
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Wasm
MYRENDERER_END_NAMESPACE  // Asset
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _WASM_TEXTURE_LOADER_
