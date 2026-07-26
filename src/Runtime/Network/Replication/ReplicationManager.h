#pragma once
#ifndef _REPLICATION_MANAGER_
#define _REPLICATION_MANAGER_

// ReplicationManager — orchestrates state synchronization between server and clients.
//
// Pattern C singleton. Maintains Map<NetObjectID, ReplicationObject*>.
//
// Server-side (authoritative):
//   Each tick, iterates all registered objects, checks dirty properties,
//   builds delta snapshots, and broadcasts via RPCDispatcher.
//
// Client-side:
//   Receives Replication envelopes, applies delta snapshots,
//   routes to InterpolationBuffer (remote objects) or PredictionController (owned objects).

#include "Core/ConstDefine.h"
#include "Network/Replication/ReplicationObject.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)

class NetworkManager;  // fwd
class BinaryBuffer;

MYRENDERER_BEGIN_NAMESPACE(Replication)

MYRENDERER_BEGIN_CLASS(ReplicationManager)
#pragma region METHOD
public:
	static void METHOD(Create)(NetworkManager* net);
	static void METHOD(Shutdown)();
	static ReplicationManager& METHOD(Get)();

	// ---- Per-frame ----
	void METHOD(Update)();

	// ---- Object registration ----
	void METHOD(RegisterObject)(NetObjectID id, ReplicationObject* obj);
	void METHOD(UnregisterObject)(NetObjectID id);
	ReplicationObject* METHOD(FindObject)(NetObjectID id) CONST;

	// ---- Server-side ----
	void METHOD(BroadcastState)();  // send dirty state to all clients

	// ---- Client-side ----
	void METHOD(ApplySnapshot)(NetObjectID obj_id, CONST UInt8* data, UInt32 len,
		PropertyMask mask);

	// ---- Role ----
	Bool METHOD(IsServer)() CONST { return m_is_server; }
	void METHOD(SetIsServer)(Bool v) { m_is_server = v; }

	// ---- Accessors ----
	NetworkManager* METHOD(GetNetwork)() CONST { return m_network; }
	UInt32 METHOD(GetObjectCount)() CONST { return (UInt32)m_objects.size(); }
protected:
private:
#pragma endregion

#pragma region MEMBER
private:
	Map<NetObjectID, ReplicationObject*> m_objects;
	Map<NetObjectID, Vector<UInt8>>      m_previous_states;  // for delta compression
	NetworkManager* m_network = nullptr;
	Bool  m_is_server = false;
	static ReplicationManager* s_instance;
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Replication
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _REPLICATION_MANAGER_
