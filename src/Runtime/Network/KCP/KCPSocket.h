#pragma once
#ifndef _KCP_SOCKET_
#define _KCP_SOCKET_

// C++ RAII wrapper around ikcp (KCP reliable UDP transport).
// KCP is a pure algorithm library — it doesn't do actual I/O.
// The user provides an output callback that sends raw UDP packets,
// feeds received data via Input(), and calls Update() periodically.

#include "Core/ConstDefine.h"
#include <functional>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(KCP)

// Callback: (data, len, user_data) — upper layer sends this via real UDP
using KCPOutputCallback = std::function<void(const void*, Int, void*)>;

MYRENDERER_BEGIN_CLASS(KCPSocket)
#pragma region METHOD
public:
	KCPSocket(UInt32 conv, void* user = nullptr);
	~KCPSocket();

	// Feed received UDP data → ikcp_input
	void  METHOD(Input)(const void* data, Int len);

	// Queue reliable data for sending → ikcp_send (>0 on success)
	Int   METHOD(Send)(const void* data, Int len);

	// Read received reliable data → ikcp_recv (call in a loop until <0)
	Int   METHOD(Recv)(void* buf, Int len);

	// Drive the clock — call every frame (or every 10ms for real-time)
	void  METHOD(Update)(UInt32 current_ms);

	// Check pending output size (for flushing before Update)
	Int   METHOD(PeekSize)() CONST;

	// Set the output callback (platform UDP send)
	void  METHOD(SetOutput)(KCPOutputCallback fn);

	// Configuration
	void  METHOD(SetWindowSize)(Int sndwnd, Int rcvwnd);
	void  METHOD(SetMTU)(Int mtu);
	void  METHOD(SetNoDelay)(Int nodelay, Int interval, Int resend, Int nc);

	// Get raw ikcp pointer (for advanced usage)
	void* METHOD(GetRaw)() { return m_kcp; }
protected:
private:
#pragma endregion

#pragma region MEMBER
public:
	KCPOutputCallback m_output;
	void* m_user = nullptr;
protected:
	void* m_kcp = nullptr;  // ikcpcb*
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // KCP
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _KCP_SOCKET_
