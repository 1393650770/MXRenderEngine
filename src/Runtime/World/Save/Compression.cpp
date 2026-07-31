#include "World/Save/Compression.h"

#if !PLATFORM_ANDROID && !PLATFORM_WASM
#include <lz4.h>
#endif

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

namespace Compression
{
	Vector<UInt8> Compress(CONST Vector<UInt8>& data)
	{
#if !PLATFORM_ANDROID && !PLATFORM_WASM
		if (data.empty())
			return {};
		Int bound = LZ4_compressBound((Int)data.size());
		Vector<UInt8> out((size_t)bound);
		Int written = LZ4_compress_default((const char*)data.data(),
			(char*)out.data(), (Int)data.size(), bound);
		if (written <= 0)
			return data;   // fall back to raw
		out.resize((size_t)written);
		return out;
#else
		return data;   // passthrough
#endif
	}

	Bool Decompress(CONST Vector<UInt8>& data, UInt32 expected_size, Vector<UInt8>& out)
	{
#if !PLATFORM_ANDROID && !PLATFORM_WASM
		out.resize(expected_size);
		Int written = LZ4_decompress_safe((const char*)data.data(),
			(char*)out.data(), (Int)data.size(), (Int)expected_size);
		if (written < 0)
			return false;
		out.resize((size_t)written);
		return true;
#else
		// Passthrough: compressed == raw on these platforms.
		if (data.size() != expected_size)
			return false;
		out = data;
		return true;
#endif
	}
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender