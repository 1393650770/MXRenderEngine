#if !PLATFORM_GLES3 && !PLATFORM_ANDROID

// WinSock includes must come before any windows.h (GLFW/Vulkan may pull it in)
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socklen_t = int;
#define SOCKET_ERR SOCKET_ERROR
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#define SOCKET_ERR (-1)
#endif

#include "Platform/Desktop/DesktopNetwork.h"
#include <iostream>
#include <cstring>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(Desktop)

DesktopNetwork::DesktopNetwork()
{
#ifdef _WIN32
	WSADATA wsa;
	m_wsa_initialized = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
#endif
#ifdef _WIN32
	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
#else
	int s = socket(AF_INET, SOCK_DGRAM, 0);
#endif
	m_socket = (SOCKET_HANDLE)s;

	if (m_socket != kInvalidSocket)
	{
#ifdef _WIN32
		u_long mode = 1; ioctlsocket((SOCKET)m_socket, FIONBIO, &mode);
#else
		fcntl((int)m_socket, F_SETFL, O_NONBLOCK);
#endif
	}
}

DesktopNetwork::~DesktopNetwork()
{
	for (auto& [conv, ch] : m_channels) { if (ch.kcp) delete ch.kcp; }
	if (m_socket != kInvalidSocket)
	{
#ifdef _WIN32
		closesocket((SOCKET)m_socket);
#else
		close((int)m_socket);
#endif
	}
#ifdef _WIN32
	if (m_wsa_initialized) WSACleanup();
#endif
}

void DesktopNetwork::HTTPGet(const String& url, HTTPCallback cb)
{
	if (cb) cb(false, "HTTP stub (Phase 4)");
}

void DesktopNetwork::HTTPPost(const String& url, const String& body, HTTPCallback cb)
{
	if (cb) cb(false, "HTTP stub (Phase 4)");
}

void DesktopNetwork::UDPOutput(const void* data, Int len, void* user)
{
	auto* self = static_cast<DesktopNetwork*>(user);
	if (self->m_socket == kInvalidSocket || !data || len <= 0) return;

	for (auto& [conv, peer] : self->m_peers)
	{
		struct sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_port = htons((u_short)peer.port);
		memcpy(&addr.sin_addr, peer.data, 4);

#ifdef _WIN32
		sendto((SOCKET)self->m_socket, static_cast<const char*>(data), len, 0,
			reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr));
#else
		sendto((int)self->m_socket, data, len, 0,
			(const struct sockaddr*)&addr, sizeof(addr));
#endif
	}
}

Bool DesktopNetwork::CreateChannel(UInt16 port)
{
	if (m_socket == kInvalidSocket) return false;

	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

#ifdef _WIN32
	bind((SOCKET)m_socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
#else
	bind((int)m_socket, (struct sockaddr*)&addr, sizeof(addr));
#endif

	auto* kcp = new Network::KCP::KCPSocket(port, this);
	kcp->SetOutput(UDPOutput);
	m_channels[port] = { kcp };
	return true;
}

void DesktopNetwork::SendTo(UInt32 conv, const void* data, Int len)
{
	auto it = m_channels.find(conv);
	if (it != m_channels.end() && it->second.kcp)
		it->second.kcp->Send(data, len);
}

Int DesktopNetwork::RecvFrom(UInt32 conv, void* buf, Int len)
{
	auto it = m_channels.find(conv);
	if (it != m_channels.end() && it->second.kcp)
		return it->second.kcp->Recv(buf, len);
	return -1;
}

void DesktopNetwork::CloseChannel(UInt32 conv)
{
	auto it = m_channels.find(conv);
	if (it != m_channels.end()) { if (it->second.kcp) delete it->second.kcp; m_channels.erase(it); }
	m_peers.erase(conv);
}

void DesktopNetwork::PollUDP()
{
	if (m_socket == kInvalidSocket) return;

	char buf[2048];
	struct sockaddr_in from = {};
	socklen_t from_len = sizeof(from);

	while (true)
	{
#ifdef _WIN32
		int n = recvfrom((SOCKET)m_socket, buf, sizeof(buf), 0,
			reinterpret_cast<struct sockaddr*>(&from), &from_len);
#else
		int n = recvfrom((int)m_socket, buf, sizeof(buf), 0,
			(struct sockaddr*)&from, &from_len);
#endif
		if (n <= 0) break;

		if (n >= 4)
		{
			UInt32 conv = *reinterpret_cast<UInt32*>(buf);
			auto it = m_channels.find(conv);
			if (it != m_channels.end() && it->second.kcp)
			{
				PeerAddr pa;
				memcpy(pa.data, &from.sin_addr, 4);
				pa.port = ntohs(from.sin_port);
				m_peers[conv] = pa;
				it->second.kcp->Input(buf + 4, n - 4);
			}
		}
	}
}

void DesktopNetwork::Update()
{
	PollUDP();
	for (auto& [conv, ch] : m_channels)
	{
		if (ch.kcp) ch.kcp->Update(0);
	}
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // !PLATFORM_GLES3
