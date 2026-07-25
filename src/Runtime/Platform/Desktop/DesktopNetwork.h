#pragma once
#ifndef _DESKTOP_NETWORK_
#define _DESKTOP_NETWORK_

#if !PLATFORM_GLES3 && !PLATFORM_ANDROID

// Desktop Network — BSD socket + KCP reliable UDP.
// WinSock includes are hidden in the .cpp to avoid windows.h conflicts.

#include "Network/NetworkSystem.h"
#include "Network/KCP/KCPSocket.h"
#include <map>
#include <functional>

// Opaque socket handle (avoids WinSock/windows.h conflicts in header)
#ifndef INVALID_SOCKET_DEFINED
#define INVALID_SOCKET_DEFINED
using SOCKET_HANDLE = uintptr_t;
static constexpr SOCKET_HANDLE kInvalidSocket = (SOCKET_HANDLE)(-1);
#endif

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(Desktop)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(DesktopNetwork, public NetworkSystem)
#pragma region METHOD
public:
	DesktopNetwork();
	VIRTUAL ~DesktopNetwork() OVERRIDE;

	VIRTUAL void METHOD(HTTPGet)(const String& url, HTTPCallback cb) OVERRIDE FINAL;
	VIRTUAL void METHOD(HTTPPost)(const String& url, const String& body, HTTPCallback cb) OVERRIDE FINAL;
	VIRTUAL void METHOD(Update)() OVERRIDE FINAL;

	Bool  METHOD(CreateChannel)(UInt16 port);
	void  METHOD(SendTo)(UInt32 conv, const void* data, Int len);
	Int   METHOD(RecvFrom)(UInt32 conv, void* buf, Int len);
	void  METHOD(CloseChannel)(UInt32 conv);

protected:
	void PollUDP();
	static void UDPOutput(const void* data, Int len, void* user);
private:
#pragma endregion

#pragma region MEMBER
protected:
	SOCKET_HANDLE m_socket = kInvalidSocket;
	Bool m_wsa_initialized = false;

	struct Channel { Network::KCP::KCPSocket* kcp = nullptr; };
	std::map<UInt32, Channel> m_channels;

	// Opaque peer address storage
	struct PeerAddr { UInt8 data[16]; UInt32 port = 0; };
	std::map<UInt32, PeerAddr> m_peers;
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // !PLATFORM_GLES3
#endif // _DESKTOP_NETWORK_
