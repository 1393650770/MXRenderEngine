#if !PLATFORM_GLES3 && !PLATFORM_ANDROID

#include "Platform/Desktop/DesktopNetwork.h"
#include <iostream>
#include <cstring>
#include <ikcp.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(Desktop)

DesktopNetwork::DesktopNetwork()
{
#ifdef _WIN32
	WSADATA wsa;
	m_wsa_initialized = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
#endif
	m_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (m_socket != INVALID_SOCKET)
	{
		// Non-blocking
#ifdef _WIN32
		u_long mode = 1; ioctlsocket(m_socket, FIONBIO, &mode);
#else
		fcntl(m_socket, F_SETFL, O_NONBLOCK);
#endif
		std::cout << "[DesktopNetwork] UDP socket created" << std::endl;
	}
}

DesktopNetwork::~DesktopNetwork()
{
	for (auto& [conv, ch] : m_channels) { if (ch.kcp) delete ch.kcp; }
	if (m_socket != INVALID_SOCKET)
	{
#ifdef _WIN32
		closesocket(m_socket);
#else
		close(m_socket);
#endif
	}
#ifdef _WIN32
	if (m_wsa_initialized) WSACleanup();
#endif
}

void DesktopNetwork::HTTPGet(const String& url, HTTPCallback cb)
{
	std::cout << "[DesktopNetwork] HTTPGet stub: " << url << std::endl;
	if (cb) cb(false, "Desktop HTTP requires libcurl (Phase 4)");
}

void DesktopNetwork::HTTPPost(const String& url, const String& body, HTTPCallback cb)
{
	if (cb) cb(false, "Desktop HTTP requires libcurl (Phase 4)");
}

void DesktopNetwork::UDPOutput(const void* data, Int len, void* user)
{
	auto* self = static_cast<DesktopNetwork*>(user);
	if (self->m_socket == INVALID_SOCKET || !data || len <= 0) return;

	// Send to all peers (simplified: broadcast per-channel)
	for (auto& [conv, peer] : self->m_peers)
	{
		sendto(self->m_socket, static_cast<const char*>(data), len, 0,
			reinterpret_cast<const struct sockaddr*>(&peer), sizeof(peer));
	}
}

Bool DesktopNetwork::CreateChannel(UInt16 port)
{
	if (m_socket == INVALID_SOCKET) return false;

	m_addr.sin_family = AF_INET;
	m_addr.sin_port = htons(port);
	m_addr.sin_addr.s_addr = INADDR_ANY;
	bind(m_socket, reinterpret_cast<struct sockaddr*>(&m_addr), sizeof(m_addr));

	// KCP conv ID = port
	auto* kcp = new Network::KCP::KCPSocket(port, this);
	kcp->SetOutput(UDPOutput);
	m_channels[port] = { kcp, m_addr };
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
	if (m_socket == INVALID_SOCKET) return;

	char buf[2048];
	struct sockaddr_in from {};
	socklen_t from_len = sizeof(from);

	while (true)
	{
		int n = recvfrom(m_socket, buf, sizeof(buf), 0,
			reinterpret_cast<struct sockaddr*>(&from), &from_len);
		if (n <= 0) break;

		// Extract KCP conv from header (first 4 bytes)
		if (n >= 4)
		{
			UInt32 conv = *reinterpret_cast<UInt32*>(buf);
			auto it = m_channels.find(conv);
			if (it != m_channels.end() && it->second.kcp)
			{
				m_peers[conv] = from;  // remember remote addr
				it->second.kcp->Input(buf + 4, n - 4);
			}
		}
	}
}

void DesktopNetwork::Update()
{
	PollUDP();
	// Drive KCP clock for all channels
	UInt32 now = 0; // TODO: use platform time
	for (auto& [conv, ch] : m_channels)
	{
		if (ch.kcp) ch.kcp->Update(now);
	}
}

MYRENDERER_END_NAMESPACE  // Desktop
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // !PLATFORM_GLES3
