#include "Network/RPC/ReliableChannel.h"
#include "Network/KCP/KCPSocket.h"
#include <algorithm>
#include <cstring>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(RPC)

// ============================================================
// KCPReliableChannel — delegates reliability to KCPSocket (KCP ARQ)
// ============================================================

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(KCPReliableChannel, public ReliableChannel)
#pragma region METHOD
public:
	KCPReliableChannel(UInt32 conv, void* user = nullptr);
	VIRTUAL ~KCPReliableChannel() OVERRIDE;

	VIRTUAL void METHOD(Send)(CONST UInt8* data, UInt32 len) OVERRIDE FINAL;
	VIRTUAL void METHOD(Receive)(CONST UInt8* data, UInt32 len) OVERRIDE FINAL;
	VIRTUAL void METHOD(Update)(UInt32 now_ms) OVERRIDE FINAL;
	VIRTUAL Bool METHOD(HasPending)() CONST OVERRIDE FINAL;
	VIRTUAL Bool METHOD(RecvReady)() CONST OVERRIDE FINAL;
	VIRTUAL Int  METHOD(RecvData)(UInt8* buf, Int buf_len) OVERRIDE FINAL;

	void   METHOD(SetOutput)(KCP::KCPOutputCallback fn);
	void*  METHOD(GetKCPRaw)() { return m_kcp ? m_kcp->GetRaw() : nullptr; }
protected:
private:
#pragma endregion

#pragma region MEMBER
private:
	KCP::KCPSocket* m_kcp = nullptr;
	Vector<UInt8>   m_recv_buf;  // temp buffer for RecvData
#pragma endregion
MYRENDERER_END_CLASS

KCPReliableChannel::KCPReliableChannel(UInt32 conv, void* user)
	: ReliableChannel(conv)
{
	m_kcp = new KCP::KCPSocket(conv, user);
	m_kcp->SetWindowSize(128, 128);
	m_kcp->SetMTU(1400);
	m_kcp->SetNoDelay(1, 10, 2, 1);
	m_recv_buf.resize(65536);
}

KCPReliableChannel::~KCPReliableChannel()
{
	delete m_kcp;
	m_kcp = nullptr;
}

void KCPReliableChannel::Send(CONST UInt8* data, UInt32 len)
{
	if (!m_kcp) return;
	m_kcp->Send(data, (Int)len);
	m_next_send_seq++;
}

void KCPReliableChannel::Receive(CONST UInt8* data, UInt32 len)
{
	if (!m_kcp) return;
	m_kcp->Input(data, (Int)len);
}

void KCPReliableChannel::Update(UInt32 now_ms)
{
	if (!m_kcp) return;
	m_kcp->Update(now_ms);
}

Bool KCPReliableChannel::HasPending() CONST
{
	if (!m_kcp) return false;
	return m_kcp->PeekSize() > 0;
}

Bool KCPReliableChannel::RecvReady() CONST
{
	if (!m_kcp) return false;
	return m_kcp->PeekSize() > 0;
}

Int KCPReliableChannel::RecvData(UInt8* buf, Int buf_len)
{
	if (!m_kcp) return -1;
	return m_kcp->Recv(buf, buf_len);
}

void KCPReliableChannel::SetOutput(KCP::KCPOutputCallback fn)
{
	if (m_kcp) m_kcp->SetOutput(std::move(fn));
}

// ============================================================
// WSReliableChannel — sliding-window ACK over WebSocket
// ============================================================

struct WSPendingSend
{
	UInt32      sequence;
	BinaryBuffer data;
	UInt32      sent_time_ms;
	UInt32      retry_count;
};

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(WSReliableChannel, public ReliableChannel)
#pragma region METHOD
public:
	WSReliableChannel(UInt32 conv);
	VIRTUAL ~WSReliableChannel() OVERRIDE;

	VIRTUAL void METHOD(Send)(CONST UInt8* data, UInt32 len) OVERRIDE FINAL;
	VIRTUAL void METHOD(Receive)(CONST UInt8* data, UInt32 len) OVERRIDE FINAL;
	VIRTUAL void METHOD(Update)(UInt32 now_ms) OVERRIDE FINAL;
	VIRTUAL Bool METHOD(HasPending)() CONST OVERRIDE FINAL;
	VIRTUAL void METHOD(GetPending)(BinaryBuffer REF out) OVERRIDE FINAL;
	VIRTUAL Bool METHOD(RecvReady)() CONST OVERRIDE FINAL;
	VIRTUAL Int  METHOD(RecvData)(UInt8* buf, Int buf_len) OVERRIDE FINAL;

	void METHOD(AckSequence)(UInt32 seq);  // process incoming ACK
protected:
private:
#pragma endregion

#pragma region MEMBER
private:
	// Send window: data waiting for ACK
	Vector<WSPendingSend> m_send_window;
	// Received buffer: ordered messages waiting for application read
	struct RecvEntry { UInt32 seq; BinaryBuffer data; };
	Vector<RecvEntry>    m_recv_queue;
	UInt32               m_last_acked = 0;

	static constexpr UInt32 kWindowSize     = 64;
	static constexpr UInt32 kResendTimeoutMs = 200;
	static constexpr UInt32 kMaxRetries     = 8;
#pragma endregion
MYRENDERER_END_CLASS

WSReliableChannel::WSReliableChannel(UInt32 conv)
	: ReliableChannel(conv)
{
}

WSReliableChannel::~WSReliableChannel() {}

void WSReliableChannel::Send(CONST UInt8* data, UInt32 len)
{
	if (m_send_window.size() >= kWindowSize) return;  // window full

	WSPendingSend entry;
	entry.sequence = m_next_send_seq++;
	entry.data.CopyFrom(data, len);
	entry.sent_time_ms = 0;
	entry.retry_count = 0;
	m_send_window.push_back(std::move(entry));
}

void WSReliableChannel::Receive(CONST UInt8* data, UInt32 len)
{
	if (len < 4) return;

	// Wire format: [sequence:4][payload...]
	UInt32 seq = *(UInt32*)data;

	// Dedup: if already received, skip
	for (auto& entry : m_recv_queue)
		if (entry.seq == seq) return;

	RecvEntry entry;
	entry.seq = seq;
	entry.data.CopyFrom(data + 4, len - 4);
	m_recv_queue.push_back(std::move(entry));

	// Sort by sequence
	std::sort(m_recv_queue.begin(), m_recv_queue.end(),
		[](CONST RecvEntry& a, CONST RecvEntry& b) { return a.seq < b.seq; });
}

void WSReliableChannel::AckSequence(UInt32 seq)
{
	m_last_acked = seq;

	// Remove all send-window entries with seq <= acked
	m_send_window.erase(
		std::remove_if(m_send_window.begin(), m_send_window.end(),
			[seq](CONST WSPendingSend& e) { return e.sequence <= seq; }),
		m_send_window.end());
}

void WSReliableChannel::Update(UInt32 now_ms)
{
	// Retransmit timed-out entries
	for (auto& entry : m_send_window)
	{
		if (entry.sent_time_ms == 0)
		{
			entry.sent_time_ms = now_ms;
		}
		// Timeout check: handled by external poll — HasPending returns true for all unsent
	}

	// Remove entries that exceeded max retries
	m_send_window.erase(
		std::remove_if(m_send_window.begin(), m_send_window.end(),
			[](CONST WSPendingSend& e) { return e.retry_count >= kMaxRetries; }),
		m_send_window.end());
}

Bool WSReliableChannel::HasPending() CONST
{
	return !m_send_window.empty();
}

void WSReliableChannel::GetPending(BinaryBuffer REF out)
{
	// Pack all pending sends into a single buffer
	if (m_send_window.empty()) return;

	UInt32 total = 0;
	for (auto& entry : m_send_window)
		total += 4 + entry.data.size;  // 4 bytes seq + payload

	out.Alloc(total);
	UInt8* ptr = out.data;
	for (auto& entry : m_send_window)
	{
		*(UInt32*)ptr = entry.sequence; ptr += 4;
		memcpy(ptr, entry.data.data, entry.data.size); ptr += entry.data.size;
	}
	out.size = total;
}

Bool WSReliableChannel::RecvReady() CONST
{
	return !m_recv_queue.empty() && m_recv_queue[0].seq == m_next_recv_seq;
}

Int WSReliableChannel::RecvData(UInt8* buf, Int buf_len)
{
	if (!RecvReady()) return -1;

	auto& entry = m_recv_queue[0];
	Int copy_len = (std::min)(buf_len, (Int)entry.data.size);
	memcpy(buf, entry.data.data, copy_len);
	m_next_recv_seq = entry.seq + 1;
	m_recv_queue.erase(m_recv_queue.begin());
	return copy_len;
}

MYRENDERER_END_NAMESPACE  // RPC
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender
