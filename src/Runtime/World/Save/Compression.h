#pragma once
#ifndef _SAVE_COMPRESSION_
#define _SAVE_COMPRESSION_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// LZ4 block compression for chunk deltas. On platforms without lz4
// (android/wasm) data passes through uncompressed (the save format is
// unchanged - the delta segment marks compression: "none").
namespace Compression
{
	// Compresses data; output may be larger than input for tiny blocks.
	Vector<UInt8> METHOD(Compress)(CONST Vector<UInt8>& data);
	// Decompresses; expected_size is the original uncompressed size.
	Bool METHOD(Decompress)(CONST Vector<UInt8>& data, UInt32 expected_size, Vector<UInt8>& out);
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _SAVE_COMPRESSION_