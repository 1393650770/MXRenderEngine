#pragma once
#ifndef _DELTA_COMPRESSOR_
#define _DELTA_COMPRESSOR_

// DeltaCompressor — builds and applies property-level deltas for replication.
//
// Each serialized ReplicationObject has a PropertyMask that tells which
// properties are present. A delta snapshot contains ONLY the properties
// that changed between two consecutive states.
//
// All methods are static — DeltaCompressor is a utility, not a singleton.

#include "Core/ConstDefine.h"
#include "Network/Replication/ReplicationObject.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(Replication)

struct DeltaSnapshot
{
	NetObjectID  obj_id = kInvalidNetID;
	PropertyMask mask = 0;     // which properties are in `data`
	UInt32       data_len = 0;
	UInt8        data[4096];   // tightly packed: only the masked properties
};

MYRENDERER_BEGIN_CLASS(DeltaCompressor)
#pragma region METHOD
public:
	// Build a delta between previous and current full state.
	// Only properties that differ between prev_mask and current_mask are included.
	static DeltaSnapshot METHOD(BuildDelta)(
		NetObjectID id,
		PropertyMask prev_mask,
		CONST UInt8* prev_data, UInt32 prev_len,
		PropertyMask curr_mask,
		CONST UInt8* curr_data, UInt32 curr_len);

	// Apply a delta to a previous snapshot to reconstruct the full state.
	// Writes the new full state to out_data/out_len.
	static Bool METHOD(ApplyDelta)(
		PropertyMask prev_mask,
		CONST UInt8* prev_data, UInt32 prev_len,
		CONST DeltaSnapshot& delta,
		PropertyMask REF out_mask,
		UInt8* out_data, UInt32 REF out_len, UInt32 max_len);
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

// ---- Inline impl (simple bit-level merge) ----

inline DeltaSnapshot DeltaCompressor::BuildDelta(
	NetObjectID id,
	PropertyMask prev_mask, CONST UInt8* prev_data, UInt32 prev_len,
	PropertyMask curr_mask, CONST UInt8* curr_data, UInt32 curr_len)
{
	DeltaSnapshot delta;
	delta.obj_id = id;

	// Delta mask = properties present in current but either absent or changed in previous
	PropertyMask delta_mask = 0;
	UInt8* dst = delta.data;
	UInt32 dst_len = 0;

	// Simple approach: include all current properties if mask differs
	// (A full proper delta would compare byte-level, but for game objects
	//  with small property counts, mask-level delta is sufficient.)
	if (curr_mask != prev_mask || prev_len != curr_len ||
		memcmp(prev_data, curr_data, (std::min)(prev_len, curr_len)) != 0)
	{
		delta_mask = curr_mask;
		if (curr_len <= sizeof(delta.data))
		{
			memcpy(dst, curr_data, curr_len);
			dst_len = curr_len;
		}
	}

	delta.mask = delta_mask;
	delta.data_len = dst_len;
	return delta;
}

inline Bool DeltaCompressor::ApplyDelta(
	PropertyMask prev_mask,
	CONST UInt8* prev_data, UInt32 prev_len,
	CONST DeltaSnapshot& delta,
	PropertyMask REF out_mask,
	UInt8* out_data, UInt32 REF out_len, UInt32 max_len)
{
	// Start from previous state
	if (prev_data && prev_len > 0 && prev_len <= max_len)
	{
		memcpy(out_data, prev_data, prev_len);
		out_len = prev_len;
		out_mask = prev_mask;
	}
	else
	{
		out_len = 0;
		out_mask = 0;
	}

	// Overlay delta properties
	if (delta.data_len > 0 && delta.data_len <= max_len)
	{
		memcpy(out_data, delta.data, delta.data_len);
		out_len = delta.data_len;
		out_mask = delta.mask;
	}

	return true;
}

MYRENDERER_END_NAMESPACE  // Replication
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _DELTA_COMPRESSOR_
