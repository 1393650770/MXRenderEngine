#pragma once
#ifndef _RENDER_COMMANDQUEUE_
#define _RENDER_COMMANDQUEUE_

// Cross-thread render command queue (UE-style ENQUEUE_RENDER_COMMAND).
//
// Two channels connect the Logic thread to the Render thread:
//   1. FrameContext snapshot (per-frame data, FrameSynchronizer)
//   2. This command queue (occasional one-shot operations)
//
// Decision rule: "does the render thread read this every frame?" -> snapshot.
// "Is this a one-shot lifecycle operation (create/destroy resource, one-time
// upload, editor action)?" -> command queue. More than ~5-10 commands per
// frame is a design error - batch or move to the snapshot.
//
// Command kinds:
//   CPU-type  (create/destroy RHI objects, flip flags) - safe everywhere.
//   Record    (Upload/Transition/Copy via RHIGetWriteCommandList()) - ONLY
//             valid when a render thread exists (bypass mode asserts).
//
// RenderCommandFence only proves the command was EXECUTED on the CPU (recorded
// into the command buffer), NOT that the GPU finished it. The only safe GPU
// visibility model: "fence complete + next frame RDG reference".
//
// Deadlock rule: RenderCommandFence::Wait() may only be called AFTER
// SignalFrameReady() (AcquireWriteSlot holds the slot while the render thread
// blocks in WaitFrameReady - spinning on a fence inside that window deadlocks).
// Command lambdas must never call FrameSynchronizer logic-thread APIs.
// Command lambdas must never call Begin()/End()/SetBypass()/SwapCommandLists().

#include "Core/ConstDefine.h"
#include "Core/ConstGlobals.h"
#include "Core/Profiling.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <thread>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)

class CommandQueue;

enum class RenderCommandFlags : UInt8
{
	None = 0,
	Record = 1,   // command body records via RHIGetWriteCommandList()
};
ENUM_CLASS_FLAGS(RenderCommandFlags)

// Fence over a command sequence number: zero-lock, trivially copyable.
// IsComplete() only means "executed on CPU by the render thread" (see header).
class RenderCommandFence
{
public:
	explicit RenderCommandFence(UInt64 seq = 0) : seq_(seq) {}

	Bool IsComplete() const;  // defined after GetRenderCommandQueue below

	// Blocking wait. Short-circuits when the queue is shutting down (returns
	// without error). Deadlock window: see header comment.
	void Wait() const;

private:
	UInt64 seq_;
};

// Thread-safe command queue consumed once per frame by the render thread.
// Bypass=true: commands execute immediately on the calling thread.
// Bypass=false: commands are enqueued, flushed by the render thread.
class CommandQueue
{
public:
	CommandQueue() = default;
	~CommandQueue() = default;
	CommandQueue(const CommandQueue&) = delete;
	CommandQueue& operator=(const CommandQueue&) = delete;

	// ---- Setup (logic thread, before StartRenderThread) ----
	void SetBypass(Bool in_bypass) { bypass_.store(in_bypass); }
	Bool IsBypass() const { return bypass_.load(); }

	// Render thread registers its id once at startup.
	void SetRenderThreadId(std::thread::id id) { render_thread_id_.store(id); }
	Bool IsRenderThread() const { return std::this_thread::get_id() == render_thread_id_.load(); }

	Bool IsRunning() const { return running_.load(); }
	void Shutdown() { running_.store(false); }

	// ---- Enqueue (any thread) ----
	// Returns the fence sequence number for this command.
	UInt64 Enqueue(const char* name, std::function<void()> cmd, RenderCommandFlags flags = RenderCommandFlags::None);
	// Raw function-pointer path for allocation-sensitive callers. Ownership of
	// payload transfers to the queue; it is delete'd after execution.
	UInt64 EnqueueRaw(const char* name, void (*fn)(void*), void* payload);

	// ---- Consume (render thread only) ----
	void Flush();   // once per frame: swap batch, execute, advance consume_seq
	void Drain();   // on render thread exit: execute remaining commands

	UInt64 ConsumeSeq() const { return consume_seq_.load(std::memory_order_acquire); }
	Bool IsComplete(UInt64 seq) const { return consume_seq_.load(std::memory_order_acquire) >= seq; }

private:
	struct Entry
	{
		const char* name = nullptr;
		std::function<void()> cmd;
		void (*fn)(void*) = nullptr;
		void* payload = nullptr;
	};

	mutable std::mutex mtx_;
	Vector<Entry> commands_;
	std::atomic<UInt64> enqueue_seq_{ 0 };
	std::atomic<UInt64> consume_seq_{ 0 };
	std::atomic<std::thread::id> render_thread_id_{};
	std::atomic<Bool> bypass_{ true };
	std::atomic<Bool> running_{ true };
};

// Global facade (function-local static, g_render_rhi pattern).
inline CommandQueue& GetRenderCommandQueue()
{
	static CommandQueue s_queue;
	return s_queue;
}

inline Bool IsInRenderThread()
{
	return GetRenderCommandQueue().IsRenderThread();
}

// Template entry point (UE EnqueueUniqueRenderCommand counterpart).
// Auto-dispatch: render thread call -> execute inline; bypass mode (no render
// thread) -> execute inline (CPU-type only, Record asserts); otherwise enqueue.
template <typename TTag, typename LAMBDA>
RenderCommandFence EnqueueRenderCommand(const char* name, LAMBDA&& lambda, RenderCommandFlags flags = RenderCommandFlags::None)
{
	CommandQueue& q = GetRenderCommandQueue();
	if (q.IsBypass() || q.IsRenderThread())
	{
		// Record-type commands need the command list in record state, which
		// only exists inside the render thread's frame - assert in bypass mode.
		CHECK_WITH_LOG(EnumHasAllFlags(flags, RenderCommandFlags::Record),
			"ENQUEUE_RENDER_COMMAND(Record) in bypass mode: no render thread, write_cb not in record state")
		lambda();
		return RenderCommandFence(q.ConsumeSeq());
	}
	return RenderCommandFence(q.Enqueue(name, std::forward<LAMBDA>(lambda), flags));
}

// Callable returned by the macro's first stage; the lambda in the second
// stage binds through operator(). UE's "Dispatcher::Enqueue<Tag>(lambda)"
// pattern: the tag is fixed, the lambda is deduced at the call site.
template <typename TTag>
struct RenderCommandEnqueueHelper
{
	explicit RenderCommandEnqueueHelper(const char* in_name) : name(in_name) {}

	template <typename LAMBDA>
	RenderCommandFence operator()(LAMBDA&& lambda) const
	{
		return EnqueueRenderCommand<TTag>(name, std::forward<LAMBDA>(lambda));
	}

	const char* name;
};

template <typename TTag>
RenderCommandEnqueueHelper<TTag> EnqueueRenderCommandNamed(const char* name)
{
	return RenderCommandEnqueueHelper<TTag>(name);
}

MYRENDERER_END_NAMESPACE  // Render
MYRENDERER_END_NAMESPACE  // MXRender

// ---- Implementation (inline, header-only queue) ----

namespace MXRender { namespace Render {

inline UInt64 CommandQueue::Enqueue(const char* name, std::function<void()> cmd, RenderCommandFlags flags)
{
	(void)flags;  // execution semantics are identical; Record is validated at enqueue time
	if (bypass_.load())
	{
		cmd();
		return consume_seq_.load(std::memory_order_acquire);
	}
	CHECK_WITH_LOG(!running_.load(), "Enqueue after CommandQueue::Shutdown")
	UInt64 seq = enqueue_seq_.fetch_add(1, std::memory_order_relaxed) + 1;
	{
		std::lock_guard<std::mutex> lock(mtx_);
		commands_.push_back(Entry{ name, std::move(cmd), nullptr, nullptr });
	}
	return seq;
}

inline UInt64 CommandQueue::EnqueueRaw(const char* name, void (*fn)(void*), void* payload)
{
	if (bypass_.load())
	{
		fn(payload);
		delete payload;
		return consume_seq_.load(std::memory_order_acquire);
	}
	CHECK_WITH_LOG(!running_.load(), "Enqueue after CommandQueue::Shutdown")
	UInt64 seq = enqueue_seq_.fetch_add(1, std::memory_order_relaxed) + 1;
	{
		std::lock_guard<std::mutex> lock(mtx_);
		commands_.push_back(Entry{ name, {}, fn, payload });
	}
	return seq;
}

inline void CommandQueue::Flush()
{
	Vector<Entry> batch;
	{
		std::lock_guard<std::mutex> lock(mtx_);
		batch.swap(commands_);
	}
	for (auto& e : batch)
	{
		PROFILE_SCOPE_DYNAMIC(e.name ? e.name : "RenderCommand")
		if (e.fn)
		{
			e.fn(e.payload);
			delete e.payload;
		}
		else if (e.cmd)
		{
			e.cmd();
		}
		consume_seq_.fetch_add(1, std::memory_order_release);
	}
}

inline void CommandQueue::Drain()
{
	Flush();
}

inline Bool RenderCommandFence::IsComplete() const
{
	return GetRenderCommandQueue().IsComplete(seq_);
}

inline void RenderCommandFence::Wait() const
{
	CommandQueue& q = GetRenderCommandQueue();
	while (!q.IsComplete(seq_))
	{
		if (!q.IsRunning())
			return;  // shutdown: no more consumption, stop spinning
		std::this_thread::yield();
	}
}

}}  // namespace MXRender::Render

// ---- UE-style two-stage macro ----
// Usage (statement-only; the first stage declares a local tag type, so the
// expansion cannot appear inside an assignment expression):
//   ENQUEUE_RENDER_COMMAND(CreateMesh)([p = std::move(payload)]{ ... });  // fire-and-forget
// When a fence is needed, call the template directly with your own tag type
// (declare it once at namespace scope):
//   struct MyCmdTag {};
//   RenderCommandFence fence = EnqueueRenderCommand<MyCmdTag>("MyCmd", [=]{ ... });
// The tag name is stringified into the profiling label.

#define MX_RENDER_COMMAND_JOIN_IMPL(A, B) A##B
#define MX_RENDER_COMMAND_JOIN(A, B) MX_RENDER_COMMAND_JOIN_IMPL(A, B)

#define ENQUEUE_RENDER_COMMAND(CommandType) \
	struct MX_RENDER_COMMAND_JOIN(MX_RENDER_COMMAND_JOIN(Local_Tag_, CommandType), MX_RENDER_COMMAND_JOIN(_, __LINE__)) {}; \
	MXRender::Render::EnqueueRenderCommandNamed<MX_RENDER_COMMAND_JOIN(MX_RENDER_COMMAND_JOIN(Local_Tag_, CommandType), MX_RENDER_COMMAND_JOIN(_, __LINE__))>(#CommandType)

// ---- Thread assertions ----
// Inverted CHECK semantics: the condition inside is the ERROR condition.
// ASSERT_RENDER_THREAD fires when NOT on the render thread (ThreeThread mode).
// ASSERT_LOGIC_THREAD fires when ON the render thread.

#define ASSERT_RENDER_THREAD() \
	CHECK_WITH_LOG((!MXRender::Render::IsInRenderThread()) && g_thread_mode >= EThreadingMode::ThreeThread, \
		"This API must be called on the render thread")
#define ASSERT_LOGIC_THREAD() \
	CHECK_WITH_LOG(MXRender::Render::IsInRenderThread(), \
		"This API must be called on the logic thread")

#endif // _RENDER_COMMANDQUEUE_
