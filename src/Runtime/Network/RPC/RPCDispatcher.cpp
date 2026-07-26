#include "Network/RPC/RPCDispatcher.h"
#include "Network/RPC/RPCService.h"
#include "Network/NetworkManager.h"
#include "Network/NetworkSystem.h"
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(RPC)

MXRender::Network::RPC::RPCDispatcher* RPCDispatcher::s_instance = nullptr;

void RPCDispatcher::Init(NetworkManager* net)
{
	if (s_instance || !net) return;
	s_instance = new RPCDispatcher();
	s_instance->m_network = net;
}

void RPCDispatcher::Shutdown()
{
	if (!s_instance) return;
	delete s_instance;
	s_instance = nullptr;
}

RPCDispatcher& RPCDispatcher::Get()
{
	static RPCDispatcher fallback;
	return s_instance ? *s_instance : fallback;
}

void RPCDispatcher::RegisterHandler(RPCID rpc_id, RPCHandler handler)
{
	if (!handler) return;
	m_handlers[rpc_id] = std::move(handler);
}

void RPCDispatcher::RegisterService(RPCService* service)
{
	if (!service) return;
	service->RegisterHandlers(this);
}

void RPCDispatcher::Dispatch(CONST UInt8* envelope_data, UInt32 len, UInt32 sender_id)
{
	if (!envelope_data || len < 8) return;

	// Minimal envelope header parsing (no FlatBuffers dependency in dispatch path):
	// Offset 0: msg_type  (ushort)
	// Offset 2: rpc_id    (ushort)
	// Offset 4: sequence  (uint)
	// Offset 8: reliable  (bool, 1 byte)
	// The payload starts after the FlatBuffers vtable offset.
	// For simplicity, we parse the full envelope using generated FlatBuffers
	// in a separate compilation unit. Here we use a minimal header peek.

	// TODO Phase 4: replace with generated NetworkEnvelope reader
	// For now, parse the 12-byte fixed header:
	UInt16 msg_type = *(UInt16*)(envelope_data + 0);
	UInt16 rpc_id   = *(UInt16*)(envelope_data + 2);
	// UInt32 sequence = *(UInt32*)(envelope_data + 4);

	if (msg_type != 0) return;  // Only handling RPC type for now

	auto it = m_handlers.find(rpc_id);
	if (it == m_handlers.end())
	{
		std::cerr << "[RPCDispatcher] No handler for RPC ID " << rpc_id << std::endl;
		return;
	}

	// Payload starts after the FlatBuffers root table offset.
	// FlatBuffers format: [root_offset:4] [vtable_offset:2] [table_data...]
	// For our NetworkEnvelope: msg_type:2, rpc_id:2, sequence:4, reliable:1, payload:vector
	// Simplified: skip 12 bytes of known header, the rest is payload.
	UInt32 header_size = 12;
	if (len <= header_size) return;

	it->second(envelope_data + header_size, len - header_size, sender_id, nullptr);
}

void RPCDispatcher::SendRPC(RPCID rpc_id, CONST UInt8* payload, UInt32 len, Bool reliable)
{
	if (!m_network || !payload || len == 0) return;

	PendingRPC pending;
	pending.rpc_id   = rpc_id;
	pending.reliable = reliable;
	pending.sequence = m_next_sequence++;
	pending.payload.CopyFrom(payload, len);

	m_pending_sends.push_back(std::move(pending));
}

void RPCDispatcher::Update()
{
	if (m_pending_sends.empty()) return;

	// Flush all pending RPCs through the network backend
	for (auto& pending : m_pending_sends)
	{
		// Serialize: build a minimal NetworkEnvelope manually for now.
		// Phase 4 will replace this with FlatBufferBuilder-based serialization.
		//
		// Minimal binary envelope format:
		// [msg_type:2][rpc_id:2][sequence:4][reliable:1][payload_len:4][payload:...]
		UInt32 envelope_size = 2 + 2 + 4 + 1 + 4 + pending.payload.size;
		Vector<UInt8> envelope(envelope_size);

		UInt32 offset = 0;
		*(UInt16*)(envelope.data() + offset) = 0; offset += 2;           // msg_type = RPC
		*(UInt16*)(envelope.data() + offset) = pending.rpc_id; offset += 2;
		*(UInt32*)(envelope.data() + offset) = pending.sequence; offset += 4;
		*(Bool*)  (envelope.data() + offset) = pending.reliable; offset += 1;
		*(UInt32*)(envelope.data() + offset) = pending.payload.size; offset += 4;

		if (pending.payload.size > 0)
		{
			memcpy(envelope.data() + offset, pending.payload.data, pending.payload.size);
		}

		// Send via the transport layer
		m_network->WebSocketSend(String(reinterpret_cast<Char*>(envelope.data()), envelope_size));
	}

	m_pending_sends.clear();
}

Bool RPCDispatcher::HasHandler(RPCID rpc_id) CONST
{
	return m_handlers.find(rpc_id) != m_handlers.end();
}

MYRENDERER_END_NAMESPACE  // RPC
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender
