#pragma once
#ifndef _RPC_DISPATCHER_
#define _RPC_DISPATCHER_

// RPCDispatcher — central RPC message dispatch hub (Pattern C singleton).
//
// Maps RPCID (UInt16) → RPCHandler (function). All network messages
// with msg_type=RPC are routed through Dispatch().
//
// Handler signature uses raw bytes + sender_id + reply callback —
// FlatBuffers types are NOT exposed to the dispatch interface.
// Deserialization happens inside each handler.
//
// Data flow:
//   [Send]  generated stub → FlatBufferBuilder → SendRPC(id, bytes, reliable)
//           → NetworkEnvelope{msg_type=RPC, rpc_id=X, payload=bytes}
//           → NetworkManager::Send(envelope)
//
//   [Recv]  NetworkManager::Update() → raw bytes arrive
//           → Dispatch(envelope_data, len, sender_id)
//             → unpack NetworkEnvelope header
//             → handler = m_handlers[rpc_id]
//             → handler(payload, payload_len, sender_id, reply_fn)

#include "Core/ConstDefine.h"
#include "Network/BinaryBuffer.h"
#include <functional>
#include <vector>
#include <unordered_map>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)

class NetworkManager;  // fwd

MYRENDERER_BEGIN_NAMESPACE(RPC)

class RPCService;  // fwd

using RPCID = UInt16;
using RPCReplyFn = std::function<void(CONST UInt8* data, UInt32 len)>;
using RPCHandler = std::function<void(
	CONST UInt8* payload, UInt32 payload_len,
	UInt32 sender_id,
	RPCReplyFn reply
)>;

struct PendingRPC
{
	RPCID       rpc_id;
	BinaryBuffer payload;
	Bool        reliable;
	UInt32      sequence;
};

MYRENDERER_BEGIN_CLASS(RPCDispatcher)
#pragma region METHOD
public:
	static void METHOD(Init)(NetworkManager* net);
	static void METHOD(Shutdown)();
	static RPCDispatcher& METHOD(Get)();

	// Register a single handler (called by generated code / RegisterHandlers)
	void METHOD(RegisterHandler)(RPCID rpc_id, RPCHandler handler);

	// Bulk-register all handlers from a service
	void METHOD(RegisterService)(RPCService* service);

	// Route an incoming raw envelope → the correct handler
	void METHOD(Dispatch)(CONST UInt8* envelope_data, UInt32 len, UInt32 sender_id);

	// Send an RPC call (payload already serialized by generated stub)
	void METHOD(SendRPC)(RPCID rpc_id, CONST UInt8* payload, UInt32 len, Bool reliable = true);

	// Per-frame: flush outgoing RPCs to the network
	void METHOD(Update)();

	NetworkManager* METHOD(GetNetwork)() CONST { return m_network; }

	// Query
	Bool METHOD(HasHandler)(RPCID rpc_id) CONST;

protected:
	void DispatchRaw(CONST UInt8* payload, UInt32 payload_len,
		UInt32 sender_id, RPCReplyFn reply);
private:
#pragma endregion

#pragma region MEMBER
private:
	Map<RPCID, RPCHandler> m_handlers;
	Vector<PendingRPC>     m_pending_sends;
	NetworkManager*        m_network = nullptr;
	UInt32                 m_next_sequence = 0;
	static RPCDispatcher*  s_instance;
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // RPC
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _RPC_DISPATCHER_
