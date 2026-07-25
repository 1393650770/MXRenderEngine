#pragma once
#ifndef _VK_QUEUE_
#define _VK_QUEUE_
#include <vulkan/vulkan_core.h>
#include "RHI/RenderResource.h"



MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

class VK_Device;
class VK_CommandBuffer;

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VK_Queue,public RenderResource)
#pragma region METHOD
public:
	VK_Queue(VK_Device* in_device,UInt32 in_family_index);
	VIRTUAL ~VK_Queue();

	void METHOD(Submit)(VK_CommandBuffer* command_list,  UInt32 num_signal_semaphores = 0, VkSemaphore* signal_semaphores = nullptr, UInt32 num_wait_semaphores = 0, VkSemaphore* wait_semaphores = nullptr);
	void METHOD(CheckCompletion)(UInt64 current_frame);
	UInt32 METHOD(GetFamily)() CONST;
	UInt32 METHOD(GetQueueIndex)() CONST;
	VkQueue METHOD(GetQueue)() CONST;
protected:

private:

#pragma endregion

#pragma region MEMBER
public:

protected:

private:
	VkQueue queue = VK_NULL_HANDLE;
	UInt32 family_index = 0;
	UInt32 queue_index = 0;
	VK_Device* device = nullptr;
	struct PendingSubmit { VkFence fence; VK_CommandBuffer* cmd; UInt64 frame_id; };
	Vector<PendingSubmit> pending_submits;
#pragma endregion


MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif //_VK_QUEUE_
