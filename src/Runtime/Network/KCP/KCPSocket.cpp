#include "Network/KCP/KCPSocket.h"
#include "kcp/ikcp.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(KCP)

// Static C callback → C++ member
static int KCPOutputBridge(const char* buf, int len, ikcpcb* kcp, void* user)
{
	auto* self = static_cast<KCPSocket*>(user);
	if (self->m_output)
	{
		self->m_output(buf, len, self->m_user);
	}
	return 0;
}

KCPSocket::KCPSocket(UInt32 conv, void* user)
	: m_user(user)
{
	m_kcp = ikcp_create(conv, this);
	if (m_kcp)
	{
		ikcp_setoutput(static_cast<ikcpcb*>(m_kcp), KCPOutputBridge);
	}
}

KCPSocket::~KCPSocket()
{
	if (m_kcp)
	{
		ikcp_release(static_cast<ikcpcb*>(m_kcp));
		m_kcp = nullptr;
	}
}

void KCPSocket::Input(const void* data, Int len)
{
	if (m_kcp && data && len > 0)
		ikcp_input(static_cast<ikcpcb*>(m_kcp), static_cast<const char*>(data), static_cast<long>(len));
}

Int KCPSocket::Send(const void* data, Int len)
{
	if (!m_kcp || !data || len <= 0) return -1;
	return ikcp_send(static_cast<ikcpcb*>(m_kcp), static_cast<const char*>(data), static_cast<int>(len));
}

Int KCPSocket::Recv(void* buf, Int len)
{
	if (!m_kcp || !buf || len <= 0) return -1;
	return ikcp_recv(static_cast<ikcpcb*>(m_kcp), static_cast<char*>(buf), static_cast<int>(len));
}

void KCPSocket::Update(UInt32 current_ms)
{
	if (m_kcp)
		ikcp_update(static_cast<ikcpcb*>(m_kcp), static_cast<UInt32>(current_ms));
}

Int KCPSocket::PeekSize() CONST
{
	if (!m_kcp) return -1;
	return ikcp_peeksize(static_cast<ikcpcb*>(m_kcp));
}

void KCPSocket::SetOutput(KCPOutputCallback fn)
{
	m_output = std::move(fn);
}

void KCPSocket::SetWindowSize(Int sndwnd, Int rcvwnd)
{
	if (m_kcp)
		ikcp_wndsize(static_cast<ikcpcb*>(m_kcp), static_cast<int>(sndwnd), static_cast<int>(rcvwnd));
}

void KCPSocket::SetMTU(Int mtu)
{
	if (m_kcp)
		ikcp_setmtu(static_cast<ikcpcb*>(m_kcp), static_cast<int>(mtu));
}

void KCPSocket::SetNoDelay(Int nodelay, Int interval, Int resend, Int nc)
{
	if (m_kcp)
		ikcp_nodelay(static_cast<ikcpcb*>(m_kcp), static_cast<int>(nodelay),
			static_cast<int>(interval), static_cast<int>(resend), static_cast<int>(nc));
}

MYRENDERER_END_NAMESPACE  // KCP
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender
