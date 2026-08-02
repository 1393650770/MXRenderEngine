#include"VK_Buffer.h"
#include "VK_Define.h"
#include "VK_Utils.h"
#include "VK_CommandBuffer.h"
#include "Core/ConstGlobals.h"
#include "VK_Memory.h"
#include "VK_Queue.h"
MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

extern enum
{
	VK_NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS = 3,
};

VK_Buffer::VK_Buffer(VK_Device* in_device, const BufferDesc& in_buffer_desc) :Buffer(in_buffer_desc),
device(in_device)
{
	ENUM_VulkanAllocationFlags alloc_flags = TranslateBufferTypeToVulkanAllocationFlags(buffer_desc.type);
	VkBufferUsageFlags usage_flags = VK_Utils::Translate_Buffer_usage_type_To_VulkanUsageFlag(buffer_desc.type);
	VkMemoryPropertyFlags mem_flags = VK_MemoryManager::GetMemoryPropertyFlags(alloc_flags,
		device->GetDeviceMemoryManager()->HasUnifiedMemory());

	// Map buffer type to allocation meta type for pooled allocation tracking
	ENUM_VK_AllocationMetaType meta_type = ENUM_VK_AllocationMetaType::EVulkanAllocationMetaMultiBuffer;
	if (EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Uniform))
		meta_type = ENUM_VK_AllocationMetaType::EVulkanAllocationMetaUniformBuffer;
	else if (EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Storage))
		meta_type = ENUM_VK_AllocationMetaType::EVulkanAllocationMetaBufferUAV;
	else if (EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Staging))
		meta_type = ENUM_VK_AllocationMetaType::EVulkanAllocationMetaBufferStaging;

	// Minimal alignment: request buffer usage flags determine alignment needs
	UInt32 min_alignment = 16;

	// Staging buffers are always dedicated (they need their own VkBuffer for Map/Unmap)
	Bool is_staging = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Staging);

	// Try shared pool allocation first (UE-style: sub-allocate from a pool VkBuffer)
	Bool pooled = !is_staging && device->GetMemoryManager()->AllocateBufferPooled(
		allocation, buffer_desc.size, min_alignment, usage_flags, mem_flags, meta_type);

	is_dedicated_buffer = !pooled;

	if (pooled)
	{
		// Use the shared pool VkBuffer; offset is pool-internal (allocation.offset)
		buffer = allocation.GetBufferHandle();
	}
	else
	{
		// Fallback: dedicated VkBuffer (for large or unusual buffers)
		VkBufferCreateInfo buffer_create_info = {};
		GenerateBufferCreateInfo(buffer_create_info, in_buffer_desc);
		vkCreateBuffer(device->GetDevice(), &buffer_create_info, VULKAN_CPU_ALLOCATOR, &buffer);

		device->GetMemoryManager()->AllocateBufferMemory(allocation, buffer, alloc_flags);
		allocation.BindBuffer(device, buffer);
	}
}

void VK_Buffer::GenerateBufferCreateInfo(VkBufferCreateInfo& buffer_create_info, const BufferDesc& desc)
{
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.size = desc.size;
	buffer_create_info.usage =VK_Utils::Translate_Buffer_usage_type_To_VulkanUsageFlag(desc.type); //VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
}

void VK_Buffer::FlushMappedMemory()
{
	allocation.FlushMappedMemory(device);
}

void VK_Buffer::Destroy()
{
	if (buffer)
	{
		VK_MemoryManager* memory_manager = device->GetMemoryManager();
		memory_manager->FreeAllocation(allocation);
		// Only destroy the VkBuffer if it's a dedicated buffer (not pooled).
		// Pooled buffers share the page's VkBuffer; destroying it here would
		// double-free after defrag replaces the pool VkBuffer (Bug D2c).
		if (is_dedicated_buffer)
		{
			vkDestroyBuffer(device->GetDevice(), buffer, VULKAN_CPU_ALLOCATOR);
		}
		buffer = VK_NULL_HANDLE;
	}
}

void* VK_Buffer::Map(CONST ENUM_MAP_TYPE& map_type, CONST ENUM_MAP_FLAG& map_flag)
{
	if (lock_state == LockState::Locked)
	{
		CHECK_WITH_LOG(true, "RHI Error :  This buffer has been locked !")
		return nullptr;
	}
	void* data = nullptr;
	lock_state = LockState::Locked;
	auto is_have_unified_memory = device->GetDeviceMemoryManager()->HasUnifiedMemory();
	Bool is_static = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Static);
	Bool is_dynamic = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Dynamic);
	Bool is_srv = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Uniform);
	Bool is_uav = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Storage);
	Bool is_staging = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Staging);
	Bool is_first_map =  0==lock_counter++;

	//CHECK_WITH_LOG(!is_staging,"RHI Error: Buffer must be created as USAGE_STAGING or USAGE_UNIFIED to be mapped for reading!")
	if (is_have_unified_memory || is_staging || is_dynamic)
	{
		data = allocation.GetMappedPointer(device);
		lock_state = LockState::PersistentMapping;
	}
	else 
	{
		UInt32 alignment = allocation.GetBufferAlignment(device);
		// --   guard-alignment-and-size
		// Alignment must be at least 1 for Align() to work correctly.
		// The fragment allocator may return 0 for dedicated allocations.
		if (alignment < 1) { alignment = 1; }
		// Staging buffer must be at least as large as the data we copy.
		UInt32 size_after_alig = std::max(Align(buffer_desc.size, alignment), buffer_desc.size);
		// --  
		VK_Buffer* staging_buffer = device->GetStagingBufferManager()->GetStagingBuffer(size_after_alig);
		VK_CommandBuffer* command_buffer = device->GetCommandBufferManager()->GetOrCreateCommandBuffer(ENUM_QUEUE_TYPE::TRANSFER);
		command_buffer->Begin();
		VkBufferCopy region;
		region.size = buffer_desc.size;
		// Use GetOffset(): for pooled buffers, this is the sub-allocation offset within the shared VkBuffer.
		// For dedicated buffers (fallback), this is 0 (offset baked into vkBindBufferMemory).
		region.srcOffset = GetOffset();
		region.dstOffset = 0;
		// --   verify-staging-size
		if (staging_buffer->GetBufferDesc().size < region.size)
		{
			std::cout << "[Map Error] staging buffer too small, recreating" << std::endl;
			device->GetStagingBufferManager()->ReleaseStagingBuffer(staging_buffer, nullptr);
			BufferDesc dedicated_desc;
			dedicated_desc.size = region.size;
			dedicated_desc.stride = 4;
			dedicated_desc.type = ENUM_BUFFER_TYPE::Staging;
			staging_buffer = new VK_Buffer(device, dedicated_desc);
		}
		// --  
		command_buffer->CopyBuffer(buffer, staging_buffer->GetBuffer(), 1, &region);
		command_buffer->MemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
		command_buffer->End();
		staging_buffer->FlushMappedMemory();
		device->GetQueue(ENUM_QUEUE_TYPE::TRANSFER)->Submit(command_buffer);
		data = staging_buffer->Map(ENUM_MAP_TYPE::Read, ENUM_MAP_FLAG::None);
		mapping.map_staging_buffer = staging_buffer;
		mapping.map_type = map_type;
	}
	return data;
}

void VK_Buffer::Unmap()
{
	Bool is_static = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Static);
	Bool is_dynamic = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Dynamic);
	Bool is_srv = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Uniform);
	Bool is_uav = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Storage);
	Bool is_staging = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Staging);
	if (lock_state != LockState::PersistentMapping)
	{
		if (mapping.map_staging_buffer != nullptr && mapping.map_type == ENUM_MAP_TYPE::Read)
		{
			device->GetStagingBufferManager()->ReleaseStagingBuffer(mapping.map_staging_buffer,nullptr);
		}
		else if (mapping.map_staging_buffer != nullptr && mapping.map_type == ENUM_MAP_TYPE::Write)
		{
			UInt32 alignment = allocation.GetBufferAlignment(device);
			// --   guard-alignment-and-size
			// Alignment must be at least 1 for Align() to work correctly.
			// The fragment allocator may return 0 for dedicated allocations.
			if (alignment < 1) { alignment = 1; }
			// Staging buffer must be at least as large as the data we copy.
			UInt32 size_after_alig = Align(buffer_desc.size, alignment);
			size_after_alig = std::max(size_after_alig, buffer_desc.size);
			// --  
			VK_CommandBuffer* command_buffer = device->GetCommandBufferManager()->GetOrCreateCommandBuffer(ENUM_QUEUE_TYPE::TRANSFER);
			command_buffer->Begin();
			VkBufferCopy region;
			region.size = buffer_desc.size;
			region.srcOffset = 0;
			region.dstOffset = GetOffset();
			command_buffer->CopyBuffer(mapping.map_staging_buffer->GetBuffer(), buffer, 1, &region);
			command_buffer->MemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
			command_buffer->End();
			device->GetQueue(ENUM_QUEUE_TYPE::TRANSFER)->Submit(command_buffer);
			device->GetStagingBufferManager()->ReleaseStagingBuffer(mapping.map_staging_buffer, nullptr);
		}
		else
		{
			CHECK_WITH_LOG(true, "RHI Error :  Invalid Unmap !")
		}
	}
	else
	{
		// Persistent mapping: make CPU writes visible to the GPU
		// (no-op on coherent memory, required on non-coherent heaps).
		FlushMappedMemory();
	}
	lock_state = LockState::Unlocked;
}

void* VK_Buffer::MapReadback(UInt32 offset, UInt32 size, Float32 timeout_seconds)
{
	// Host-visible buffers (Staging|Dynamic): return the persistent mapping.
	Bool is_dynamic = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Dynamic);
	Bool is_staging = EnumHasAnyFlags(buffer_desc.type, ENUM_BUFFER_TYPE::Staging);
	if (is_dynamic || is_staging)
	{
		void* ptr = allocation.GetMappedPointer(device);
		if (!ptr)
			return nullptr;
		return (UInt8*)ptr + offset;
	}

	// Device-local: private staging + transfer copy + BLOCK until complete.
	// Uses vkQueueWaitIdle on the graphics queue first (the producing queue),
	// then submits the copy and waits for the transfer queue to finish.
	// Debug/save-time use only - never per frame.
	BufferDesc staging_desc;
	staging_desc.size = size;
	staging_desc.stride = 4;
	staging_desc.type = ENUM_BUFFER_TYPE::Staging;
	VK_Buffer* staging = new VK_Buffer(device, staging_desc);
	if (!staging)
		return nullptr;

	// Wait for all graphics work to finish so the copy reads settled data.
	VkQueue graphics_queue = device->GetQueue(ENUM_QUEUE_TYPE::GRAPHICS)->GetQueue();
	vkQueueWaitIdle(graphics_queue);

	VK_CommandBuffer* cmd = device->GetCommandBufferManager()->GetOrCreateCommandBuffer(ENUM_QUEUE_TYPE::TRANSFER);
	cmd->Begin();
	VkBufferCopy region;
	region.size = size;
	region.srcOffset = GetOffset() + offset;
	region.dstOffset = 0;
	cmd->CopyBuffer(buffer, staging->GetBuffer(), 1, &region);
	cmd->MemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
	cmd->End();

	VK_Queue* transfer_queue = device->GetQueue(ENUM_QUEUE_TYPE::TRANSFER);
	transfer_queue->Submit(cmd);

	// Wait for the transfer queue to drain (blocking readback).
	VkQueue transfer_vk_queue = transfer_queue->GetQueue();
	vkQueueWaitIdle(transfer_vk_queue);

	void* data = staging->Map(ENUM_MAP_TYPE::Read, ENUM_MAP_FLAG::None);
	if (!data)
	{
		delete staging;
		return nullptr;
	}

	// Store the staging in the mapping so FreeReadback can release it.
	mapping.map_staging_buffer = staging;
	mapping.map_type = ENUM_MAP_TYPE::Read;
	return data;
}

void VK_Buffer::FreeReadback(void* data)
{
	(void)data;
	if (mapping.map_staging_buffer)
	{
		mapping.map_staging_buffer->Unmap();
		delete mapping.map_staging_buffer;
		mapping.map_staging_buffer = nullptr;
	}
}


ENUM_VulkanAllocationFlags VK_Buffer::TranslateBufferTypeToVulkanAllocationFlags(const ENUM_BUFFER_TYPE& buffer_usage)
{
	// Usage bits may be combined (Vertex|Dynamic, Storage|Indirect, ...): every
	// buffer is auto-bound and host visibility is expressed solely by the
	// Staging/Dynamic bits. The Dynamic bit requests a host-visible allocation
	// (persistent map for per-frame CPU uploads - the staging+transfer path is
	// NOT safe for buffers read by another queue every frame).
	ENUM_VulkanAllocationFlags allocation_flags = ENUM_VulkanAllocationFlags::AutoBind;
	if (EnumHasAnyFlags(buffer_usage, ENUM_BUFFER_TYPE::Staging | ENUM_BUFFER_TYPE::Dynamic))
		allocation_flags = allocation_flags | ENUM_VulkanAllocationFlags::HostVisible;
	return allocation_flags;
}

VK_Buffer::~VK_Buffer()
{
	Destroy();
}

VkBuffer VK_Buffer::GetBuffer() CONST
{
	// Dedicated buffers: the cached 'buffer' member is the ground truth
	// (allocation.vulkan_handle is not set for dedicated allocations).
	// Pooled buffers: read allocation.GetBufferHandle() dynamically — after
	// defrag moves sub-allocations to a new pool VkBuffer, the cached value
	// would be stale (Bug D2b).
	if (is_dedicated_buffer)
		return buffer;
	return allocation.GetBufferHandle();
}

UInt32 VK_Buffer::GetOffset() CONST
{
	// For pooled buffers: return sub-allocation offset within the shared pool VkBuffer.
	// For dedicated buffers (fallback): allocation offset was baked into vkBindBufferMemory.
	if (allocation.GetBufferHandle() != VK_NULL_HANDLE)
		return allocation.GetBufferOffset();
	return 0;
}

Bool VK_Buffer::CanMove() CONST
{
	return true;
}

VkBuffer VK_Buffer::GetVkBuffer() CONST
{
	return buffer;
}

UInt32 VK_Buffer::GetMemorySize() CONST
{
	return buffer_desc.size;
}

void VK_Buffer::Move(VK_Device& device, VK_CommandBuffer* cmd, VK_Allocation& new_allocation)
{
	// 1. GPU copy old buffer -> new buffer
	VkBufferCopy region;
	region.srcOffset = 0;
	region.dstOffset = 0;
	region.size = buffer_desc.size;
	cmd->CopyBuffer(buffer, new_allocation.GetBufferHandle(), 1, &region);

	// 2. Bind this buffer to the new memory
	vkBindBufferMemory(device.GetDevice(), buffer, new_allocation.GetDeviceMemoryHandle(&device), new_allocation.offset);

	// 3. Swap allocations - old goes to new_allocation for caller to free
	allocation.Swap(new_allocation);
}

VK_StagingBufferManager::VK_StagingBufferManager(VK_Device* in_device) :device(in_device)
{

}

void VK_StagingBufferManager::ProcessPendingFree(Bool is_immediate, Bool is_free_to_os)
{
	Int ori_free_buffer_size = free_buffers.size();
	for (Int i = 0; i < pending_items_per_cmd_buffer.size(); i++)
	{
		PendingItemPerCmdBuffer& pending_item = pending_items_per_cmd_buffer[i];
		for(Int fence_index = 0; fence_index < pending_item.pending_items.size(); fence_index++)
		{
			PendingItemPerCmdBuffer::PendingItems& pending_item_per_fence = pending_item.pending_items[fence_index];
			if (is_immediate ||pending_item_per_fence.fence < pending_item.command_buffer->GetFenceSignaledCounter())
			{
				for (Int buffer_index = 0; buffer_index < pending_item_per_fence.buffer.size(); buffer_index++)
				{
					VK_Buffer* buffer = pending_item_per_fence.buffer[buffer_index];
					free_buffers.push_back({ buffer, g_frame_number_render_thread.load() });
				}
				pending_item_per_fence.buffer.clear();
				pending_item.pending_items.erase(pending_item.pending_items.begin() + fence_index);
				fence_index--;
			}
		}
	}

	if (is_free_to_os)
	{
		for (Int i = 0; i < free_buffers.size(); i++)
		{
			if (is_immediate ||free_buffers[i].frame_num + VK_NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS < g_frame_number_render_thread.load())
			{
				free_buffers[i].buffer->Destroy();
				delete free_buffers[i].buffer;
				free_buffers.erase(free_buffers.begin() + i);
				i--;
			}
		}
	}
}

void VK_StagingBufferManager::ReleaseStagingBuffer(VK_Buffer*& buffer, VK_CommandBuffer* command_buffer)
{
	if (command_buffer)
	{
		PendingItemPerCmdBuffer* ItemsForCmdBuffer = FindOrAddPendingItemPerCmdBuffer(command_buffer);
		PendingItemPerCmdBuffer::PendingItems* ItemsForFence = ItemsForCmdBuffer->FindOrAddItemsForFence(command_buffer->GetFenceSignaledCounter());
		ItemsForFence->buffer.push_back(buffer);
	}
	else
	{
		free_buffers.push_back({ buffer, g_frame_number_render_thread.load() });
	}
	buffer = nullptr;
}

VK_Buffer* VK_StagingBufferManager::GetStagingBuffer(UInt64 size)
{
	// --   fix-staging-remove
	// Use iterator instead of range-for + std::remove. std::remove reorders
	// elements which would invalidate a range-for reference mid-iteration.
	for (auto it = free_buffers.begin(); it != free_buffers.end(); ++it)
	{
		if (it->buffer->GetBufferDesc().size >= size)
		{
			VK_Buffer* result = it->buffer;
			free_buffers.erase(it);
			return result;
		}
	}
	// --  
	BufferDesc staging_buffer_desc;

	staging_buffer_desc.size = size;
	staging_buffer_desc.stride = 4;
	staging_buffer_desc.type = ENUM_BUFFER_TYPE::Staging;
	VK_Buffer* staging_buffer = new VK_Buffer(device, staging_buffer_desc);
	using_buffers.push_back(staging_buffer);
	return staging_buffer;
}

VK_StagingBufferManager::PendingItemPerCmdBuffer* VK_StagingBufferManager::FindOrAddPendingItemPerCmdBuffer(VK_CommandBuffer* command_buffer)
{
	for (auto& pending_item : pending_items_per_cmd_buffer)
	{
		if (pending_item.command_buffer == command_buffer)
		{
			return &pending_item;
		}
	}
	PendingItemPerCmdBuffer new_pending_item;
	new_pending_item.command_buffer = command_buffer;
	pending_items_per_cmd_buffer.push_back(new_pending_item);
	return &pending_items_per_cmd_buffer.back();

}

VK_StagingBufferManager::~VK_StagingBufferManager()
{
	ProcessPendingFree(true, true);
}

inline VK_StagingBufferManager::PendingItemPerCmdBuffer::PendingItems* VK_StagingBufferManager::PendingItemPerCmdBuffer::FindOrAddItemsForFence(UInt64 fence)
{
	for (auto& pending_item : pending_items)
	{
		if (pending_item.fence == fence)
		{
			return &pending_item;
		}
	}
	PendingItems new_pending_item;
	new_pending_item.fence = fence;
	pending_items.push_back(new_pending_item);
	return &pending_items.back();
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE