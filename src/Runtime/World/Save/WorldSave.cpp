#include "World/Save/WorldSave.h"
#include "World/Save/Compression.h"
#include "Platform/PlatformFile.h"
#include <nlohmann/json.hpp>
#include <cstring>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

namespace
{
	using json = nlohmann::json;

	// ---- varint helpers ----
	void WriteVarint(Vector<UInt8>& out, UInt32 value)
	{
		while (value >= 0x80)
		{
			out.push_back((UInt8)(value & 0x7F) | 0x80);
			value >>= 7;
		}
		out.push_back((UInt8)value);
	}

	Bool ReadVarint(CONST Vector<UInt8>& in, UInt32& pos, UInt32& out)
	{
		out = 0;
		UInt32 shift = 0;
		while (pos < in.size() && shift < 35)
		{
			UInt8 byte = in[pos++];
			out |= (UInt32)(byte & 0x7F) << shift;
			if ((byte & 0x80) == 0)
				return true;
			shift += 7;
		}
		return false;
	}

	// Delta encoding: pairs of (index_delta, packed_cell).
	// index_delta is the difference from the previous changed index - most
	// edits are small, so varint keeps the delta tiny.
	void EncodeDelta(CONST Vector<UInt32>& cells, CONST Vector<UInt32>& baseline,
		Vector<UInt8>& out, Bool& has_delta)
	{
		UInt32 prev_index = 0;
		Bool first = true;
		for (UInt32 i = 0; i < cells.size(); ++i)
		{
			if (cells[i] == baseline[i])
				continue;
			has_delta = true;
			UInt32 index_delta = first ? i : (i - prev_index);
			WriteVarint(out, index_delta);
			WriteVarint(out, cells[i]);
			prev_index = i;
			first = false;
		}
	}

	Bool DecodeDelta(CONST Vector<UInt8>& in, UInt32 cell_count,
		CONST Vector<UInt32>& baseline, Vector<UInt32>& out)
	{
		out = baseline;
		UInt32 pos = 0;
		UInt32 index = 0;
		Bool first = true;
		while (pos < in.size())
		{
			UInt32 index_delta = 0;
			UInt32 value = 0;
			if (!ReadVarint(in, pos, index_delta) || !ReadVarint(in, pos, value))
				return false;
			index = first ? index_delta : (index + index_delta);
			if (index >= cell_count)
				return false;
			out[index] = value;
			first = false;
		}
		return true;
	}

	String Base64(CONST Vector<UInt8>& data)
	{
		static CONST char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		String out;
		out.reserve(((data.size() + 2) / 3) * 4);
		for (size_t i = 0; i < data.size(); i += 3)
		{
			UInt32 v = (UInt32)data[i] << 16;
			if (i + 1 < data.size()) v |= (UInt32)data[i + 1] << 8;
			if (i + 2 < data.size()) v |= (UInt32)data[i + 2];
			out.push_back(table[(v >> 18) & 0x3F]);
			out.push_back(table[(v >> 12) & 0x3F]);
			out.push_back(i + 1 < data.size() ? table[(v >> 6) & 0x3F] : '=');
			out.push_back(i + 2 < data.size() ? table[v & 0x3F] : '=');
		}
		return out;
	}

	Vector<UInt8> Unbase64(CONST String& in)
	{
		Vector<UInt8> out;
		auto val = [](char c) -> Int {
			if (c >= 'A' && c <= 'Z') return c - 'A';
			if (c >= 'a' && c <= 'z') return c - 'a' + 26;
			if (c >= '0' && c <= '9') return c - '0' + 52;
			if (c == '+') return 62;
			if (c == '/') return 63;
			return -1;
		};
		UInt32 buf = 0;
		Int bits = 0;
		for (char c : in)
		{
			if (c == '=')
				break;
			Int v = val(c);
			if (v < 0)
				continue;
			buf = (buf << 6) | (UInt32)v;
			bits += 6;
			if (bits >= 8)
			{
				bits -= 8;
				out.push_back((UInt8)((buf >> bits) & 0xFF));
			}
		}
		return out;
	}
}

// ---- Public API ----

void SaveWorldToFile(CONST WorldSave& save, CONST String& path)
{
	json root;
	root["format_version"] = save.format_version;
	root["seed"] = save.seed;
	root["tick"] = save.tick;
	root["material_version"] = save.material_version;
	root["generation_version"] = save.generation_version;
	root["next_entity_id"] = save.next_entity_id;

	json chunks = json::array();
	for (CONST auto& delta : save.chunk_deltas)
	{
		json j;
		j["key"] = { delta.key.cx, delta.key.cy };
		j["baseline_version"] = delta.baseline_version;
		j["has_delta"] = delta.has_delta;
		if (delta.has_delta)
		{
			Vector<UInt8> compressed = Compression::Compress(delta.compressed_delta);
			j["delta"] = Base64(compressed);
		}
		chunks.push_back(j);
	}
	root["chunks"] = chunks;

	// Edit log (debug/verification only).
	json edits = json::array();
	for (CONST auto& e : save.edit_log)
		edits.push_back({ e.x, e.y, e.material, e.op });
	root["edit_log"] = edits;

	String json_str = root.dump();
	Vector<UInt8> bytes(json_str.begin(), json_str.end());
	Platform::PlatformFile::WriteFileAtomic(path, bytes);
}

Bool LoadWorldFromFile(CONST String& path, WorldSave& out)
{
	Vector<UInt8> bytes;
	if (!Platform::PlatformFile::ReadFile(path, bytes))
		return false;

	json root;
	try
	{
		root = json::parse(String(bytes.begin(), bytes.end()));
	}
	catch (...)
	{
		return false;
	}

	out.format_version = root.value("format_version", 1);
	out.seed = root.value("seed", 0u);
	out.tick = root.value("tick", 0ull);
	out.material_version = root.value("material_version", 1u);
	out.generation_version = root.value("generation_version", 1u);
	out.next_entity_id = root.value("next_entity_id", 1ull);

	out.chunk_deltas.clear();
	if (root.contains("chunks"))
	{
		for (CONST auto& j : root["chunks"])
		{
			ChunkDelta delta;
			delta.key.cx = j["key"][0];
			delta.key.cy = j["key"][1];
			delta.baseline_version = j.value("baseline_version", 1u);
			delta.has_delta = j.value("has_delta", false);
			if (delta.has_delta && j.contains("delta"))
				delta.compressed_delta = Unbase64(j["delta"].get<String>());
			out.chunk_deltas.push_back(delta);
		}
	}

	out.edit_log.clear();
	if (root.contains("edit_log"))
	{
		for (CONST auto& e : root["edit_log"])
		{
			EditEvent edit;
			edit.x = e[0];
			edit.y = e[1];
			edit.material = e[2];
			edit.op = e[3];
			out.edit_log.push_back(edit);
		}
	}
	return true;
}

// ---- Delta encode/decode helpers (used by SaveManager) ----

Vector<UInt8> EncodeChunkDelta(CONST Vector<UInt32>& cells, CONST Vector<UInt32>& baseline, Bool& has_delta)
{
	Vector<UInt8> out;
	EncodeDelta(cells, baseline, out, has_delta);
	return out;
}

Bool DecodeChunkDelta(CONST Vector<UInt8>& compressed, UInt32 cell_count,
	CONST Vector<UInt32>& baseline, Vector<UInt32>& out)
{
	Vector<UInt8> raw;
	if (!Compression::Decompress(compressed, 0, raw))
		return false;
	// Decompress with unknown size: try decoding the raw buffer directly.
	// (The save path always stores the compressed form; empty compressed
	//  means no delta.)
	if (compressed.empty())
	{
		out = baseline;
		return true;
	}
	return DecodeDelta(raw, cell_count, baseline, out);
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender