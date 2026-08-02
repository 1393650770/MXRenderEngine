#include "Render/Core/RenderFrameSync.h"
#include "Render/Core/CommandQueue.h"
#include "Render/RenderInterface.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderViewport.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)

FrameSynchronizer::FrameSynchronizer()
{
}

FrameSynchronizer::~FrameSynchronizer()
{
	if (render_running.load(std::memory_order_acquire))
	{
		StopRenderThread();
	}
}

// === Logic Thread API ===

FrameContext* FrameSynchronizer::AcquireWriteSlot()
{
	std::unique_lock<std::mutex> lock(mtx);
	cv_logic.wait(lock, [this]() {
		return slot_states[write_index] == SlotState::Free
			|| !render_running.load(std::memory_order_acquire);
	});

	if (!render_running.load(std::memory_order_acquire))
		return nullptr;

	slot_states[write_index] = SlotState::LogicWriting;
	return &contexts[write_index];
}

void FrameSynchronizer::SignalFrameReady()
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		slot_states[write_index] = SlotState::Ready;
		frames_in_flight++;
		write_index = (write_index + 1) % kMaxFramesInFlight;
	}
	cv_render.notify_one();
}

// NOTE: WaitFrameComplete is the LOCKSTEP debug path (MX_FORCE_LOCKSTEP=1).
// The default ThreeThread flow uses TryRecycleCompleted at frame start and
// blocks only in AcquireWriteSlot (all 3 slots busy = back-pressure).
FrameContext* FrameSynchronizer::WaitFrameComplete()
{
	std::unique_lock<std::mutex> lock(mtx);
	cv_complete.wait(lock, [this]() {
		return slot_states[complete_index] == SlotState::Done || !render_running.load(std::memory_order_acquire);
	});

	if (!render_running.load(std::memory_order_acquire))
		return nullptr;

	FrameContext* result = &contexts[complete_index];
	slot_states[complete_index] = SlotState::Free;
	frames_in_flight--;
	complete_index = (complete_index + 1) % kMaxFramesInFlight;
	cv_logic.notify_one();
	return result;
}

Bool FrameSynchronizer::TryRecycleCompleted()
{
	std::lock_guard<std::mutex> lock(mtx);
	if (slot_states[complete_index] != SlotState::Done)
		return false;

	slot_states[complete_index] = SlotState::Free;
	if (frames_in_flight > 0)
		frames_in_flight--;
	complete_index = (complete_index + 1) % kMaxFramesInFlight;
	// MUST wake a logic thread blocked in AcquireWriteSlot (all 3 slots busy
	// while the render thread finishes - missing this notify deadlocks the
	// back-pressure path).
	cv_logic.notify_one();
	return true;
}

// === Render Thread API ===

FrameContext* FrameSynchronizer::WaitFrameReady()
{
	std::unique_lock<std::mutex> lock(mtx);
	cv_render.wait(lock, [this]() {
		return slot_states[render_index] == SlotState::Ready || !render_running.load(std::memory_order_acquire);
	});

	if (!render_running.load(std::memory_order_acquire))
		return nullptr;

	slot_states[render_index] = SlotState::RenderProcessing;
	return &contexts[render_index];
}

void FrameSynchronizer::SignalRenderDone()
{
	{
		std::lock_guard<std::mutex> lock(mtx);
		slot_states[render_index] = SlotState::Done;
		render_index = (render_index + 1) % kMaxFramesInFlight;

		// Recycle eagerly: a logic thread blocked in AcquireWriteSlot (all 3
		// slots busy = back-pressure) must be woken THIS frame - the logic
		// thread's own TryRecycleCompleted only runs at frame start, so
		// waiting for it would deadlock (logic blocked, render done, no one
		// recycles). The Done check under the same mutex makes double-recycle
		// impossible (recycling flips the slot to Free).
		if (slot_states[complete_index] == SlotState::Done)
		{
			slot_states[complete_index] = SlotState::Free;
			if (frames_in_flight > 0)
				frames_in_flight--;
			complete_index = (complete_index + 1) % kMaxFramesInFlight;
		}
	}
	cv_complete.notify_one();
	cv_logic.notify_one();   // wake a logic thread blocked in AcquireWriteSlot
}

// === Render Thread Lifecycle ===

Bool FrameSynchronizer::IsRenderRunning() CONST
{
	return render_running.load(std::memory_order_acquire);
}

void FrameSynchronizer::StartRenderThread(RenderInterface* render, RHI::Viewport* viewport)
{
	render_running.store(true, std::memory_order_release);
	render_thread = std::thread(&FrameSynchronizer::RenderThreadMain, this, render, viewport);
}

void FrameSynchronizer::StopRenderThread()
{
	render_running.store(false, std::memory_order_release);
	cv_logic.notify_one();
	cv_render.notify_one();
	cv_complete.notify_one();
	if (render_thread.joinable())
	{
		render_thread.join();
	}
}

void FrameSynchronizer::JoinRenderThread()
{
	if (render_thread.joinable())
	{
		render_thread.join();
	}
}

//  Render thread main loop
void FrameSynchronizer::RenderThreadMain(RenderInterface* render, RHI::Viewport* viewport)
{
	// Init render resources on the Render thread
	render->OnInit_Render();

	// Register this thread's id so command-queue enqueues auto-dispatch inline.
	GetRenderCommandQueue().SetRenderThreadId(std::this_thread::get_id());

	while (render_running.load(std::memory_order_acquire))
	{
		FrameContext* ctx = WaitFrameReady();
		if (!ctx) continue;

		if (ctx->needs_resize && viewport)
		{
			viewport->Resize(ctx->resize_width, ctx->resize_height);
			ctx->needs_resize = false;
		}

		auto* cmd_list = RHIGetWriteCommandList();
		cmd_list->SetBypass(false);
		cmd_list->Begin();

		// Flush logic-thread commands HERE: after Begin (so Record-type bodies
		// can append to the recording list), before OnPreRender (so resources
		// created this frame are available to graph.Execute below). Recorded
		// commands replay in order [Begin][flush][OnPreRender][OnRender]...
		GetRenderCommandQueue().Flush();

		render->OnPreRender(*ctx);

		// Record scene passes (no ImGui — UIPass removed from RG)
		render->OnRender();

		// Record ImGui GPU commands (draw data from Logic thread)
		render->OnPostRender(*ctx);

		cmd_list->SetBypass(true);

		RHISwapCommandLists();

		// Wait for the RHI thread to finish replaying THIS frame (frame-number
		// sync). The boolean flag alone can signal the previous frame's replay
		// completion, letting us present a frame whose commands were never
		// replayed - silently dropping uploads/dispatches recorded that frame.
		UInt64 my_frame = g_render_rhi->GetSwapFrame();
		while (g_render_rhi->GetReplayFrame() < my_frame)
			std::this_thread::yield();

		if (viewport)
		{
			auto* present_cb = RHIGetRHICmdListForPresent();
			viewport->Present(present_cb, true, true);
		}

		RHIRenderEnd();

		SignalRenderDone();
	}

	// Execute any commands enqueued after the last flush, then shut the queue
	// down. Order matters: Drain BEFORE OnShutdown_Render (commands may still
	// reference render resources), and Shutdown() so fence.Wait short-circuits.
	GetRenderCommandQueue().Drain();
	GetRenderCommandQueue().Shutdown();

	// Cleanup render resources before exiting
	render->OnShutdown_Render();
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
