#pragma once
#ifndef _PIXEL_WORLD_EVENT_QUEUE_
#define _PIXEL_WORLD_EVENT_QUEUE_

#include "Core/ConstDefine.h"
#include "World/ITerrainEditSink.h"
#include <mutex>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Buffer;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Command queue for CPU -> GPU edit events. Thread-safe: the logic thread
// enqueues (brush input), the render thread flushes inside the simulation
// pass execute lambda (single consumer). Events are double-buffered at the
// GPU side (frame parity) so an in-flight frame never reads a buffer being
// rewritten.
MYRENDERER_BEGIN_CLASS(PixelWorldEventQueue)
#pragma region METHOD
public:
	PixelWorldEventQueue();
	~PixelWorldEventQueue() MYDEFAULT;

	void METHOD(Enqueue)(CONST EditEvent& edit);
	void METHOD(Reset)();
	UInt32 METHOD(GetPendingCount)() CONST;
	Bool METHOD(HasPending)() CONST;

	// Writes count + events into dst (a Storage|Dynamic buffer). The count
	// lives in the first 8 bytes of the buffer ({count, pad}); events follow.
	// Returns the number of events written (may be less than pending if the
	// buffer capacity is exceeded - oldest events are dropped).
	UInt32 METHOD(FlushTo)(RHI::Buffer* dst, UInt32 max_events);

	// Number of events flushed last frame (render thread). Used by the chain
	// gate: run the chain even with 0 pending ticks when the GPU still holds
	// unconsumed events.
	UInt32 last_flushed_ = 0;

protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	mutable std::mutex mutex_;
	Vector<EditEvent> pending_;
	UInt32 pending_count_ = 0;
	static CONST UInt32 kMaxPending = 16384;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _PIXEL_WORLD_EVENT_QUEUE_