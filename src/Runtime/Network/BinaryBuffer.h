#pragma once
#ifndef _BINARY_BUFFER_
#define _BINARY_BUFFER_

// BinaryBuffer — lightweight byte container for network message payloads.
// Pure value type: owns or references a UInt8* buffer.
// NOT a BinaryWriter/Reader — FlatBuffers handles serialization;
// BinaryBuffer is the transport container between serialization and the wire.
//
// Usage:
//   // Take ownership from FlatBufferBuilder::Release()
//   BinaryBuffer buf;
//   buf.Detach(builder.Release().data(), builder.GetSize());
//   NetworkManager::Send(buf.data, buf.size);
//   buf.Release();

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)

MYRENDERER_BEGIN_STRUCT(BinaryBuffer)
public:
	void METHOD(Alloc)(UInt32 size);
	void METHOD(CopyFrom)(CONST UInt8* src, UInt32 len);
	void METHOD(Detach)(UInt8* src, UInt32 len);  // take ownership
	void METHOD(Release)();
	void METHOD(Reset)();

	// Inline convenience
	Bool METHOD(IsEmpty)() CONST { return size == 0; }
	Bool METHOD(IsValid)() CONST { return data != nullptr; }

	UInt8* data = nullptr;
	UInt32 capacity = 0;
	UInt32 size = 0;
	Bool   owns = false;
MYRENDERER_END_STRUCT

// ---- Inline impl ----

inline void BinaryBuffer::Alloc(UInt32 in_size)
{
	Release();
	data = new UInt8[in_size];
	capacity = in_size;
	size = 0;
	owns = true;
}

inline void BinaryBuffer::CopyFrom(CONST UInt8* src, UInt32 len)
{
	if (capacity < len) { Alloc(len); }
	memcpy(data, src, len);
	size = len;
}

inline void BinaryBuffer::Detach(UInt8* src, UInt32 len)
{
	Release();
	data = src;
	size = len;
	capacity = len;
	owns = true;
}

inline void BinaryBuffer::Release()
{
	if (owns && data) { delete[] data; }
	data = nullptr;
	capacity = 0;
	size = 0;
	owns = false;
}

inline void BinaryBuffer::Reset()
{
	size = 0;
}

MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _BINARY_BUFFER_
