#include "World/PixelWorldEventQueue.h"
#include "Tool/BufferUtils.h"
#include <algorithm>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

PixelWorldEventQueue::PixelWorldEventQueue()
{
	pending_.reserve(kMaxPending);
}

void PixelWorldEventQueue::Enqueue(CONST EditEvent& edit)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (pending_.size() >= kMaxPending)
	{
		// Drop oldest to make room (overwrite at front).
		pending_.erase(pending_.begin());
	}
	pending_.push_back(edit);
	++pending_count_;
}

void PixelWorldEventQueue::Reset()
{
	std::lock_guard<std::mutex> lock(mutex_);
	pending_.clear();
	pending_count_ = 0;
	last_flushed_ = 0;
}

UInt32 PixelWorldEventQueue::GetPendingCount() CONST
{
	std::lock_guard<std::mutex> lock(mutex_);
	return pending_count_;
}

Bool PixelWorldEventQueue::HasPending() CONST
{
	std::lock_guard<std::mutex> lock(mutex_);
	return pending_count_ > 0;
}

UInt32 PixelWorldEventQueue::FlushTo(RHI::Buffer* dst, UInt32 max_events)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (pending_.empty() || dst == nullptr)
		return 0;

	UInt32 to_write = pending_.size() < max_events ? (UInt32)pending_.size() : max_events;

	// Buffer layout: {UInt32 count; UInt32 pad; EditEvent evts[]}
	struct Header { UInt32 count; UInt32 pad; };
	Header header{ to_write, 0 };
	Tool::BufferUtils::Upload(dst, &header, sizeof(header), 0);
	Tool::BufferUtils::Upload(dst, pending_.data(), to_write * sizeof(EditEvent), sizeof(header));

	// Consume flushed events (keep the tail if we dropped any).
	if (to_write < pending_.size())
		pending_.erase(pending_.begin(), pending_.begin() + to_write);
	else
		pending_.clear();

	pending_count_ = 0;   // CPU mirror: all flushed events are now pending on the GPU
	last_flushed_ = to_write;
	return to_write;
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender