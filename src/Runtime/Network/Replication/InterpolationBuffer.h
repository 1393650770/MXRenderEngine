#pragma once
#ifndef _INTERPOLATION_BUFFER_
#define _INTERPOLATION_BUFFER_

// InterpolationBuffer — ring buffer of timestamped state snapshots.
//
// Used by the client to smooth remote objects. Snapshots arrive from the
// server at fixed intervals (e.g. 20Hz). The render frame (60+ Hz) samples
// between the two most recent snapshots bracketing (render_time - delay).
//
// delay_ms: how far behind real-time we render remote objects (default 50ms).
//   Larger = smoother, more latency.

#include "Core/ConstDefine.h"
#include "Network/Replication/ReplicationObject.h"
#include <algorithm>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(Replication)

struct TimedSnapshot
{
	Float64 timestamp = 0.0;
	PropertyMask mask = 0;
	UInt8  data[4096];
	UInt32 data_len = 0;
};

MYRENDERER_BEGIN_CLASS(InterpolationBuffer)
#pragma region METHOD
public:
	InterpolationBuffer() MYDEFAULT;

	void METHOD(PushSnapshot)(Float64 timestamp, CONST UInt8* data,
		UInt32 len, PropertyMask mask);

	Bool METHOD(GetInterpolatedState)(Float64 render_time,
		UInt8* out_data, UInt32 REF out_len, Float32 REF out_t,
		PropertyMask REF out_mask) CONST;

	void METHOD(SetInterpolationDelay)(Float64 delay_ms) { m_delay_ms = delay_ms; }
	Float64 METHOD(GetInterpolationDelay)() CONST { return m_delay_ms; }

	void METHOD(Clear)();
	Bool METHOD(IsEmpty)() CONST { return m_count == 0; }
	UInt32 METHOD(GetCount)() CONST { return m_count; }
protected:
private:
#pragma endregion

#pragma region MEMBER
private:
	static constexpr UInt32 kRingSize = 64;
	TimedSnapshot m_ring[kRingSize];
	UInt32 m_head = 0;
	UInt32 m_count = 0;
	Float64 m_delay_ms = 50.0;
#pragma endregion
MYRENDERER_END_CLASS

// ---- Inline impl ----

inline void InterpolationBuffer::PushSnapshot(
	Float64 timestamp, CONST UInt8* data, UInt32 len, PropertyMask mask)
{
	TimedSnapshot& snap = m_ring[m_head];
	snap.timestamp = timestamp;
	snap.mask = mask;
	snap.data_len = (std::min)(len, (UInt32)sizeof(snap.data));
	if (data && snap.data_len > 0)
		memcpy(snap.data, data, snap.data_len);

	m_head = (m_head + 1) % kRingSize;
	if (m_count < kRingSize) m_count++;
}

inline Bool InterpolationBuffer::GetInterpolatedState(
	Float64 render_time,
	UInt8* out_data, UInt32 REF out_len, Float32 REF out_t,
	PropertyMask REF out_mask) CONST
{
	if (m_count < 2) return false;

	Float64 sample_time = render_time - m_delay_ms;

	// Find the two snapshots bracketing sample_time
	// (snapshots are in chronological order from (head-count) to (head-1))
	UInt32 start = (m_head + kRingSize - m_count) % kRingSize;

	CONST TimedSnapshot* before = nullptr;
	CONST TimedSnapshot* after = nullptr;

	for (UInt32 i = 0; i < m_count; i++)
	{
		UInt32 idx = (start + i) % kRingSize;
		if (m_ring[idx].timestamp <= sample_time)
			before = &m_ring[idx];
		else
		{
			after = &m_ring[idx];
			break;
		}
	}

	if (!before || !after) return false;

	Float64 range = after->timestamp - before->timestamp;
	if (range <= 0.0) return false;

	out_t = (Float32)((sample_time - before->timestamp) / range);
	out_t = (std::max)(0.0f, (std::min)(1.0f, out_t));

	// Simple linear interpolation: copy the "before" state
	// (Proper interpolation requires per-property lerp — deferred to Phase 4 generated code)
	out_len = before->data_len;
	out_mask = before->mask;
	if (before->data_len > 0)
		memcpy(out_data, before->data, before->data_len);

	return true;
}

inline void InterpolationBuffer::Clear()
{
	m_count = 0;
	m_head = 0;
}

MYRENDERER_END_NAMESPACE  // Replication
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _INTERPOLATION_BUFFER_
