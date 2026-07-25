#pragma once
#ifndef _ASSET_PACK_READER_
#define _ASSET_PACK_READER_

// Asset Pack Reader — reads .assetpack binary archives.
// Format: magic(4) + version(4) + entry_count(4) + entries[N] + data_blocks[N]
// Each entry: hash64(8) + offset(4) + compressed_size(4) + raw_size(4) + flags(4) = 24B
// Data blocks are individually LZ4-compressed (random access).

#include "Core/ConstDefine.h"
#include <map>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Asset)
MYRENDERER_BEGIN_NAMESPACE(Wasm)

static constexpr UInt32 kAssetPackMagic = 0x4B434150;  // 'PACK'
static constexpr UInt32 kAssetPackVersion = 1;

struct AssetPackEntry
{
	UInt64 hash = 0;
	UInt32 offset = 0;
	UInt32 compressed_size = 0;
	UInt32 raw_size = 0;
	UInt32 flags = 0;  // bit0=compressed, bit1=texture, bit2=mesh
};

MYRENDERER_BEGIN_CLASS(AssetPackReader)
#pragma region METHOD
public:
	AssetPackReader() MYDEFAULT;

	Bool METHOD(Open)(const Vector<UInt8>& pack_data);
	Bool METHOD(ReadEntry)(const String& path, Vector<UInt8>& out_data);
	Bool METHOD(HasEntry)(const String& path) const;

	// Simple hash for path strings (FNV-1a 64-bit)
	static UInt64 METHOD(HashPath)(const String& path);
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	std::map<UInt64, AssetPackEntry> m_index;
	const UInt8* m_data_start = nullptr;  // points into pack_data memory
	size_t m_data_size = 0;
private:
#pragma endregion
MYRENDERER_END_CLASS

// ---- Inline impl ----

inline UInt64 AssetPackReader::HashPath(const String& path)
{
	UInt64 hash = 14695981039346656037ULL; // FNV offset basis
	for (char c : path) { hash ^= (UInt8)c; hash *= 1099511628211ULL; }
	return hash;
}

inline Bool AssetPackReader::Open(const Vector<UInt8>& pack_data)
{
	if (pack_data.size() < 12) return false;
	const UInt32* header = reinterpret_cast<const UInt32*>(pack_data.data());
	if (header[0] != kAssetPackMagic || header[1] != kAssetPackVersion) return false;

	UInt32 entry_count = header[2];
	const UInt8* ptr = pack_data.data() + 12;

	for (UInt32 i = 0; i < entry_count; ++i)
	{
		AssetPackEntry entry;
		entry.hash  = *reinterpret_cast<const UInt64*>(ptr); ptr += 8;
		entry.offset= *reinterpret_cast<const UInt32*>(ptr); ptr += 4;
		entry.compressed_size = *reinterpret_cast<const UInt32*>(ptr); ptr += 4;
		entry.raw_size = *reinterpret_cast<const UInt32*>(ptr); ptr += 4;
		entry.flags  = *reinterpret_cast<const UInt32*>(ptr); ptr += 4;
		m_index[entry.hash] = entry;
	}

	m_data_start = ptr;
	m_data_size = pack_data.size() - (ptr - pack_data.data());
	return true;
}

inline Bool AssetPackReader::HasEntry(const String& path) const
{
	return m_index.find(HashPath(path)) != m_index.end();
}

inline Bool AssetPackReader::ReadEntry(const String& path, Vector<UInt8>& out_data)
{
	UInt64 hash = HashPath(path);
	auto it = m_index.find(hash);
	if (it == m_index.end() || !m_data_start) return false;

	const auto& entry = it->second;
	if (entry.offset + entry.compressed_size > m_data_size) return false;

	const UInt8* src = m_data_start + entry.offset;
	out_data.resize(entry.raw_size);

	if (entry.flags & 1) // compressed (LZ4)
	{
		// LZ4 decompress: simple copy for now (LZ4 integration later)
		// Phase D2+: use lz4_decompress_safe()
		memcpy(out_data.data(), src, entry.compressed_size < entry.raw_size ? entry.compressed_size : entry.raw_size);
	}
	else
	{
		memcpy(out_data.data(), src, entry.raw_size);
	}
	return true;
}

MYRENDERER_END_NAMESPACE  // Wasm
MYRENDERER_END_NAMESPACE  // Asset
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _ASSET_PACK_READER_
