#pragma once
#ifndef _RELIABLE_CHANNEL_
#define _RELIABLE_CHANNEL_

// ReliableChannel — abstract base for reliable message delivery.
//
// Pattern A (abstract base, virtual empty defaults).
// Two concrete implementations:
//   - KCPReliableChannel: delegates to KCPSocket (ARQ over UDP, Desktop)
//   - WSReliableChannel:  sliding-window ACK over WebSocket (Browser/MiniGame)
//
// Both provide the same interface: Send/Receive/Update with sequencing.

#include "Core/ConstDefine.h"
#include "Network/BinaryBuffer.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(RPC)

MYRENDERER_BEGIN_CLASS(ReliableChannel)
#pragma region METHOD
public:
	ReliableChannel(UInt32 conv) : m_conv(conv) {}
	VIRTUAL ~ReliableChannel() MYDEFAULT;

	// Queue data for reliable delivery
	VIRTUAL void METHOD(Send)(CONST UInt8* data, UInt32 len) {}

	// Feed received raw data into the channel
	VIRTUAL void METHOD(Receive)(CONST UInt8* data, UInt32 len) {}

	// Drive internal state (ARQ timers, retransmission)
	VIRTUAL void METHOD(Update)(UInt32 now_ms) {}

	// Get next outgoing sequence number
	VIRTUAL UInt32 METHOD(GetNextSequence)() CONST { return m_next_send_seq; }

	// Check if there is pending outgoing data
	VIRTUAL Bool METHOD(HasPending)() CONST { return false; }

	// Retrieve pending outgoing data (for external send loop)
	VIRTUAL void METHOD(GetPending)(BinaryBuffer REF out) {}

	// Check if received data is ready to read
	VIRTUAL Bool METHOD(RecvReady)() CONST { return false; }

	// Read a reliably delivered message (returns -1 if none available)
	VIRTUAL Int METHOD(RecvData)(UInt8* buf, Int buf_len) { return -1; }

	UInt32 METHOD(GetConv)() CONST { return m_conv; }
protected:
	UInt32 m_conv;
	UInt32 m_next_send_seq = 0;
	UInt32 m_next_recv_seq = 0;
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // RPC
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _RELIABLE_CHANNEL_
