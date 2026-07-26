#pragma once
#ifndef _REPLICATION_OBJECT_
#define _REPLICATION_OBJECT_

// ReplicationObject — base class for any object that needs network replication.
//
// Server-side:
//   GetDirtyProperties() → which PropertyMask bits changed
//   Serialize(buf, len, max_len, mask) → pack dirty properties into buffer
//   ClearDirtyProperties() → reset dirty tracking after broadcast
//
// Client-side:
//   Deserialize(data, len, mask) → apply authoritative state from server
//   Interpolate(from, to, t) → smooth remote object rendering
//   ApplyPredicted(prediction) → reconcile client prediction with server state
//
// Generated ReplicationTraits<T> (Phase 4) provides:
//   - PropertyMask constants (kFieldNameMask = 1 << N)
//   - SerializeProperties / DeserializeProperties / GetDirtyMask

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(Replication)

using NetObjectID = UInt32;
static constexpr NetObjectID kInvalidNetID = 0;
using PropertyMask = UInt32;
static constexpr UInt32 kMaxReplicatedProperties = 32;

MYRENDERER_BEGIN_CLASS(ReplicationObject)
#pragma region METHOD
public:
	ReplicationObject() MYDEFAULT;
	VIRTUAL ~ReplicationObject() MYDEFAULT;

	// ---- Identity ----
	NetObjectID METHOD(GetNetID)() CONST { return m_net_id; }
	void METHOD(SetNetID)(NetObjectID id) { m_net_id = id; }
	Bool METHOD(IsReplicated)() CONST { return m_net_id != kInvalidNetID; }

	// ---- Server: dirty tracking ----
	// Returns a bitmask where bit N = property N has changed.
	// Generated code overrides this.
	VIRTUAL PropertyMask METHOD(GetDirtyProperties)() CONST { return 0; }
	VIRTUAL void METHOD(ClearDirtyProperties)() {}

	// ---- Server: serialization ----
	// Serialize only the properties indicated by `mask` into buffer.
	VIRTUAL void METHOD(Serialize)(UInt8* buf, UInt32 REF len, UInt32 max_len, PropertyMask mask) {}

	// ---- Client: deserialization ----
	// Apply an authoritative state snapshot from the server.
	VIRTUAL void METHOD(Deserialize)(CONST UInt8* data, UInt32 len, PropertyMask mask) {}

	// ---- Client: interpolation (remote objects) ----
	// Interpolate between two snapshots at time t (0..1).
	VIRTUAL void METHOD(Interpolate)(CONST ReplicationObject* from, CONST ReplicationObject* to, Float32 t) {}

	// ---- Client: prediction (locally owned objects) ----
	VIRTUAL void METHOD(ApplyPredicted)(CONST ReplicationObject* prediction) {}

	// ---- Lifecycle ----
	VIRTUAL void METHOD(OnSpawn)() {}
	VIRTUAL void METHOD(OnDespawn)() {}
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	NetObjectID m_net_id = kInvalidNetID;
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Replication
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _REPLICATION_OBJECT_
