#pragma once
#ifndef _DESKTOP_NETWORK_
#define _DESKTOP_NETWORK_

#if !PLATFORM_GLES3 && !PLATFORM_ANDROID

// Desktop Network — BSD socket + KCP reliable UDP.
// Windows: WinSock2, Linux: POSIX sockets.

#include "Network/NetworkSystem.h"
#include "Network/KCP/KCPSocket.h"
#include <map>
#include <functional>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_t = int;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
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

	// KCP reliable channel: open UDP socket + wrap with KCP
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
	SOCKET m_socket = INVALID_SOCKET;
	struct sockaddr_in m_addr {};
	Bool m_wsa_initialized = false;

	struct Channel { Network::KCP::KCPSocket* kcp = nullptr; struct sockaddr_in addr {}; };
	std::map<UInt32, Channel> m_channels;
	std::map<UInt32, struct sockaddr_in> m_peers;  // conv → remote addr
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Desktop
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // !PLATFORM_GLES3
#endif // _DESKTOP_NETWORK_
