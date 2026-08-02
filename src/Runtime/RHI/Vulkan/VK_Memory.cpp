#include"VK_Memory.h"
#include "VK_Define.h"
#include "VK_Device.h"
#include "VK_Utils.h"
#include "Core/ConstGlobals.h"
#include "Core/TypeHash.h"
#include "VK_CommandBuffer.h"
#include "VK_Queue.h"
#include <mutex>
#ifdef _MSC_VER
#include <xutility>
#endif

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

// Deferred VkBuffer destruction (Bug D2a).
// DefragTick moves live sub-allocations to a new pool VkBuffer and must
// NOT destroy the old one immediately — the transfer-queue copy may still
// be executing, and VK_Buffer objects still cache the old handle.
namespace {
	struct DeferredVkBuffer { VkBuffer buffer; VkDeviceMemory memory; UInt64 frame; };
	Vector<DeferredVkBuffer> g_deferred_vk_buffers;
	std::mutex g_deferred_vk_mutex;
} // anon namespace

static void DeferredDestroyVkBuffer(VkDevice device, VkBuffer buf, VkDeviceMemory mem = VK_NULL_HANDLE)
{
	if (buf == VK_NULL_HANDLE) return;
	std::lock_guard<std::mutex> lock(g_deferred_vk_mutex);
	g_deferred_vk_buffers.push_back({buf, mem, g_frame_number_render_thread.load()});
}

void ProcessDeferredVkBuffers(VkDevice device)
{
	std::lock_guard<std::mutex> lock(g_deferred_vk_mutex);
	UInt64 safe = g_frame_number_render_thread.load() >= 3
		? g_frame_number_render_thread.load() - 3 : 0;
	for (Int i = (Int)g_deferred_vk_buffers.size() - 1; i >= 0; --i)
	{
		if (g_deferred_vk_buffers[i].frame <= safe)
		{
			vkDestroyBuffer(device, g_deferred_vk_buffers[i].buffer, VULKAN_CPU_ALLOCATOR);
			if (g_deferred_vk_buffers[i].memory != VK_NULL_HANDLE)
				vkFreeMemory(device, g_deferred_vk_buffers[i].memory, VULKAN_CPU_ALLOCATOR);
			g_deferred_vk_buffers[i] = g_deferred_vk_buffers.back();
			g_deferred_vk_buffers.pop_back();
		}
	}
}


#define VK_MEMORY_MAX_SUB_ALLOCATION (64llu << 20llu) //64MB?
#define VK_MEMORY_KEEP_FREELIST_SORTED	1
#define VK_MEMORY_JOIN_FREELIST_ON_THE_FLY	(VK_MEMORY_KEEP_FREELIST_SORTED && 1)
#define VK_MEMORY_KEEP_FREELIST_SORTED_CATCHBUGS			0 // debugging

enum
{
	VK_NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS = 3,
};

UInt32 g_vulkan_budget_percentage_scale = 100;
Int g_vulkan_use_buffer_binning = 1;
static VkMemoryPropertyFlags InternalGetMemoryPropertyFlags(ENUM_VulkanAllocationFlags alloc_flags, Bool is_has_unified_memory)
{
	VkMemoryPropertyFlags mem_flags = 0;

	//checkf(!(EnumHasAnyFlags(AllocFlags, ENUM_VulkanAllocationFlags::PreferBAR) && !EnumHasAnyFlags(AllocFlags, ENUM_VulkanAllocationFlags::HostVisible)), TEXT("PreferBAR should always be used with HostVisible."));

	if (EnumHasAnyFlags(alloc_flags, ENUM_VulkanAllocationFlags::HostCached))
	{
		mem_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	}
	else if (EnumHasAnyFlags(alloc_flags, ENUM_VulkanAllocationFlags::HostVisible))
	{
		mem_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

		if (EnumHasAnyFlags(alloc_flags, ENUM_VulkanAllocationFlags::PreferBAR))
		{
			mem_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		}
	}
	else
	{
		mem_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		if (is_has_unified_memory)
		{
			mem_flags |= (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		}
	}

	if (EnumHasAnyFlags(alloc_flags, ENUM_VulkanAllocationFlags::Memoryless))
	{
		mem_flags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
	}

	return mem_flags;
}
static UInt32 CalculateBufferAlignmentFromVKUsageFlags(VK_Device* in_device, CONST VkBufferUsageFlags in_buffer_usage_flags)
{
	CONST VkPhysicalDeviceLimits& Limits = in_device->GetLimits();

	CONST Bool is_texelbuffer = VK_Utils::VKHasAnyFlags(in_buffer_usage_flags, (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT));
	CONST Bool is_storagebuffer = VK_Utils::VKHasAnyFlags(in_buffer_usage_flags, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	CONST Bool is_vertex_or_indexbuffer = VK_Utils::VKHasAnyFlags(in_buffer_usage_flags, (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT));
	CONST Bool is_acceleration_structurebuffer = VK_Utils::VKHasAnyFlags(in_buffer_usage_flags, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
	CONST Bool is_uniformbuffer = VK_Utils::VKHasAnyFlags(in_buffer_usage_flags, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	// Buffers are sometimes directly cast into classes with 16byte alignment expectations (like FVector3f)
	UInt32 alignment = 16u;

	if (is_texelbuffer || is_storagebuffer)
	{
		alignment = std::max(alignment, (UInt32)Limits.minTexelBufferOffsetAlignment);
		alignment = std::max(alignment, (UInt32)Limits.minStorageBufferOffsetAlignment);
	}
	else if (is_vertex_or_indexbuffer)
	{
		// No alignment restrictions on Vertex or Index buffers, leave it at 1
	}
	//else if (bIsAccelerationStructureBuffer)
	//{
	//	alignment = std::max(alignment, GRHIRayTracingAccelerationStructureAlignment);
	//}
	else if (is_uniformbuffer)
	{
		alignment = std::max(alignment, (UInt32)Limits.minUniformBufferOffsetAlignment);
	}
	// else: TRANSFER_SRC/DST, INDIRECT, or other general-purpose buffers — keep default 16-byte alignment

	return alignment;
}
Bool MetaTypeCanEvict(ENUM_VK_AllocationMetaType meta_type)
{
	switch (meta_type)
	{
	case ENUM_VK_AllocationMetaType::EVulkanAllocationMetaImageOther:
		return true;
	default:
		return false;
	}
}

static Float32 CalculateBufferPriority(CONST VkBufferUsageFlags in_buffer_usage_flags)
{
	float priority = VULKAN_MEMORY_MEDIUM_PRIORITY;

	if (VK_Utils::VKHasAnyFlags(in_buffer_usage_flags, (VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT)))
	{
		priority = VULKAN_MEMORY_HIGHEST_PRIORITY;
	}
	else if (VK_Utils::VKHasAnyFlags(in_buffer_usage_flags, (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)))
	{
		priority = VULKAN_MEMORY_MEDIUM_PRIORITY;
	}
	else if (VK_Utils::VKHasAnyFlags(in_buffer_usage_flags,
		(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
			VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR)))
	{
		priority = VULKAN_MEMORY_MEDIUM_PRIORITY;
	}
	else if (VK_Utils::VKHasAnyFlags(in_buffer_usage_flags, (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT)))
	{
		priority = VULKAN_MEMORY_LOW_PRIORITY;
	}
	else if (VK_Utils::VKHasAnyFlags(in_buffer_usage_flags, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
	{
		priority = VULKAN_MEMORY_HIGHER_PRIORITY;
	}
	else
	{
		// TRANSFER_SRC/DST or other general-purpose buffers — default medium priority
		priority = VULKAN_MEMORY_MEDIUM_PRIORITY;
	}

	return priority;
}

VK_DeviceMemoryAllocation::VK_DeviceMemoryAllocation()
{
    property.packed=0;
}

void* VK_DeviceMemoryAllocation::Map(VkDeviceSize in_size, VkDeviceSize offset)
{
    CHECK_WITH_LOG(property.is_can_be_mapped==false,"RHI Error: This Memory can not be mapped when Map!")
    if(!mapped_pointer)
    {
        CHECK_WITH_LOG(vkMapMemory(device_handle,memory,offset,in_size,0,&mapped_pointer)!=VK_SUCCESS,"RHI Error: This Memory map failed!")
    }
    return mapped_pointer;
}

void VK_DeviceMemoryAllocation::UnMap()
{
    if(mapped_pointer)
    {
        vkUnmapMemory(device_handle,memory);
        mapped_pointer=nullptr;
    }
}

VkDeviceMemory VK_DeviceMemoryAllocation::GetMemory()
{
    return memory;
}

VkDeviceSize VK_DeviceMemoryAllocation::GetSize()
{
    return size;
}

void* VK_DeviceMemoryAllocation::GetMappedPointer()
{
    if(property.is_can_be_mapped&&mapped_pointer)
    {
        return mapped_pointer;
    }
    return nullptr;
}

void VK_DeviceMemoryAllocation::FlushMappedMemory(VkDeviceSize in_size, VkDeviceSize offset)
{
    if(property.is_coherent==false)
    {
        CHECK_WITH_LOG(mapped_pointer==nullptr,"RHI Error: This Memory can not be mapped when Flush !")
        CHECK_WITH_LOG(in_size+offset>size,"RHI Error: Flush Mapped Memory is out size !")
        VkMappedMemoryRange range;
        range.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory=memory;
        range.offset=offset;
        range.size=in_size;
        
        CHECK_WITH_LOG(vkFlushMappedMemoryRanges(device_handle, 1, &range)!=VK_SUCCESS,"RHI Error: Flush Mapped Memory failed !")
    }
}

void VK_DeviceMemoryAllocation::InvalidateMappedMemory(VkDeviceSize in_size, VkDeviceSize offset)
{
    if(property.is_coherent==false)
    {
        CHECK_WITH_LOG(mapped_pointer==nullptr,"RHI Error: This Memory can not be mapped when Flush !")
        CHECK_WITH_LOG(in_size+offset>size,"RHI Error: Flush Mapped Memory is out size !")
        VkMappedMemoryRange range;
        range.sType=VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory=memory;
        range.offset=offset;
        range.size=in_size;
        
        CHECK_WITH_LOG(vkInvalidateMappedMemoryRanges(device_handle, 1, &range)!=VK_SUCCESS,"RHI Error: Invalidate Mapped Memory failed !")
    }
}

Bool VK_DeviceMemoryAllocation::GetIsCanBeMapped() CONST
{
    return property.is_can_be_mapped;
}

Bool VK_DeviceMemoryAllocation::GetIsCoherent() CONST
{
    return property.is_coherent;
}

Bool VK_DeviceMemoryAllocation::CheckIsMapped() CONST
{
	return mapped_pointer!=nullptr;
}

void VK_DeviceMemoryManager::Destroy()
{
	TrimMemory(true);
	for (Int index = 0; index < memory_heaps.size(); ++index)
	{
	CHECK_WITH_LOG(memory_heaps[index].allocations.size(),
			("RHI Info: Found " + std::to_string(memory_heaps[index].allocations.size()) + " unfree allocation !").c_str())
	}
	num_allocations = 0;
}


VK_DeviceMemoryManager::VK_DeviceMemoryManager(VK_Device* in_device):device(in_device)
{
    VkPhysicalDeviceMemoryProperties2 properties2;
    memory_budget.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
	memory_budget.pNext = nullptr;
    properties2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
    properties2.pNext=&memory_budget;

	vkGetPhysicalDeviceMemoryProperties2(device->GetGpu(), &properties2);

    memcpy(&memory_properties,&(properties2.memoryProperties),sizeof(VkPhysicalDeviceMemoryProperties));
   
	for (UInt32 index= 0; index<VK_MAX_MEMORY_HEAPS;++index)
    {
        memory_budget.heapBudget[index]=g_vulkan_budget_percentage_scale*  memory_budget.heapBudget[index] / 100;
    }

    VkDeviceSize budget_size = 0;
    for (UInt32 index = 6; index < VK_MAX_MEMORY_HEAPS; ++index)
    {
        budget_size += memory_budget.heapBudget[index];
    }

    //vkGetPhysicalDeviceMemoryProperties(device->GetGpu(), &memory_properties);

    primary_heap_index = -1;
    UInt64 PrimaryHeapSize = 0;
    UInt32 NonLocalHeaps = 0;

    for(UInt32 i = 0; i < memory_properties.memoryHeapCount; ++i)
    {
			
        if (VKHasAllFlags(memory_properties.memoryHeaps[i].flags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
        {
            if(memory_properties.memoryHeaps[i].size > PrimaryHeapSize)
            {
                primary_heap_index = i;
                PrimaryHeapSize = memory_properties.memoryHeaps[i].size;
            }
        }
        else
        {
            NonLocalHeaps++;
        }
    }
    if(0 == NonLocalHeaps)
    {
        primary_heap_index = -1; 
    }

    // Update bMemoryless support
    is_support_lazily_allocated=false;
    for (UInt32 i = 0; i < memory_properties.memoryTypeCount && !is_support_lazily_allocated; ++i)
    {
        is_support_lazily_allocated = VKHasAllFlags(memory_properties.memoryTypes[i].propertyFlags, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
    }

    memory_heaps.resize(memory_properties.memoryHeapCount);

}

VK_DeviceMemoryManager::~VK_DeviceMemoryManager()
{
	Destroy();
}

VK_DeviceMemoryAllocation* VK_DeviceMemoryManager::Alloc(VkDeviceSize allocation_size, UInt32 memory_type_index,
    void* dedicated_allocate_info, float priority, Bool is_external)
{
    if (dedicated_allocate_info==nullptr)
    {
        MemoryBlockKey key(memory_type_index, allocation_size);
        MemoryBlock& block = memory_block_map[key];
		block.key = key;
        if(block.allocations.size() > 0)
        {
            MemoryBlock::FreeBlock alloc = block.allocations.back();
            block.allocations.pop_back();
            return alloc.allocation;
        }
    }
    VkMemoryAllocateInfo allocate_info;
    allocate_info.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize=allocation_size;
    allocate_info.memoryTypeIndex=memory_type_index;
	allocate_info.pNext = nullptr;
#if VULKAN_SUPPORTS_MEMORY_PRIORITY
    VkMemoryPriorityAllocateInfoEXT prio;
    if (device->GetOptionalExtensions().HasMemoryPriority)
    {
        prio.sType= VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT;
        prio.priority = priority;
        allocate_info.pNext = &prio;
    }
#endif
    
#if VULKAN_SUPPORTS_DEDICATED_ALLOCATION
    if (dedicated_allocate_info)
    {
        ((VkMemoryDedicatedAllocateInfoKHR*)dedicated_allocate_info)->pNext = allocate_info.pNext;
        allocate_info.pNext = dedicated_allocate_info;
    }
#endif
    VkMemoryAllocateFlagsInfo memory_allocate_flags_info;
    if (device->GetOptionalExtensions().HasBufferDeviceAddress)
    {
        memory_allocate_flags_info.sType= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        memory_allocate_flags_info.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
        memory_allocate_flags_info.pNext = allocate_info.pNext;
        allocate_info.pNext = &memory_allocate_flags_info;
    }

    VkDeviceMemory new_memory;
    VkResult result;
    result = vkAllocateMemory(device->GetDevice(), &allocate_info, VULKAN_CPU_ALLOCATOR, &new_memory);
    CHECK_WITH_LOG(result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_OUT_OF_HOST_MEMORY,"RHI Error : Cannot allocate Memory ! ");
    VK_DeviceMemoryAllocation* memory_allocation = new VK_DeviceMemoryAllocation;
    memory_allocation->device_handle = device->GetDevice();
    memory_allocation->memory = new_memory;
    memory_allocation->size = allocation_size;
    memory_allocation->property.memory_type_index = memory_type_index;
    memory_allocation->property.is_can_be_mapped = VKHasAllFlags(memory_properties.memoryTypes[memory_type_index].propertyFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    memory_allocation->property.is_coherent = VKHasAllFlags(memory_properties.memoryTypes[memory_type_index].propertyFlags, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    memory_allocation->property.is_cached = VKHasAllFlags(memory_properties.memoryTypes[memory_type_index].propertyFlags, VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
#if VULKAN_SUPPORTS_DEDICATED_ALLOCATION
    NewAllocation->property.is_dedicated_memory = dedicated_allocate_info != nullptr;
#endif
    ++num_allocations;
    max_num_allocations=std::max(num_allocations,max_num_allocations);

    //if (num_allocations == device->GetLimits().maxMemoryAllocationCount)


    UInt32 heap_index = memory_properties.memoryTypes[memory_type_index].heapIndex;
    memory_heaps[heap_index].allocations.push_back(memory_allocation);
    memory_heaps[heap_index].used_size += allocation_size;
    memory_heaps[heap_index].max_size = std::max(memory_heaps[heap_index].max_size, memory_heaps[heap_index].used_size);
    return memory_allocation;
}


void VK_DeviceMemoryManager::Free(VK_DeviceMemoryAllocation*& allocation)
{
	if (allocation && allocation->property.is_dedicated_memory == false)
	{
		VkDeviceSize allocation_size = allocation->size;
		MemoryBlockKey key(allocation->property.memory_type_index, allocation_size);

		MemoryBlock& block = memory_block_map[key];
		block.key = key;
		MemoryBlock::FreeBlock free_block = { allocation, g_frame_number_render_thread.load() };
		block.allocations.push_back(free_block);
		return;
	}
	FreeInternal(allocation);
	
}


CONST UInt32 VK_DeviceMemoryManager::GetMemoryTypeNum() CONST
{
    return memory_properties.memoryTypeCount;
}

UInt32 VK_DeviceMemoryManager::GetHeapIndex(UInt32 memory_type_index)
{
	return memory_properties.memoryTypes[memory_type_index].heapIndex;
}
CONST VkPhysicalDeviceMemoryProperties& VK_DeviceMemoryManager::GetMemoryProperties() CONST
{
    return memory_properties;
}

VkResult VK_DeviceMemoryManager::GetMemoryTypeFromProperties(UInt32 type_bits, VkMemoryPropertyFlags properties, UInt32* out_type_index)
{
	for (UInt32 i = 0; i < memory_properties.memoryTypeCount && type_bits; ++i)
	{
		if ((type_bits & 1) == 1)
		{
			if ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				*out_type_index = i;
				return VK_SUCCESS;
			}
		}
		type_bits >>= 1;
	}
	return VK_ERROR_FEATURE_NOT_PRESENT;
}

Bool VK_DeviceMemoryManager::GetIsSupportUnifiedMemory() CONST
{
    return is_support_unified_memory;
}

VkResult VK_DeviceMemoryManager::GetMemoryTypeFromPropertiesExcluding(UInt32 type_bits, VkMemoryPropertyFlags properties, UInt32 exclude_type_index, UInt32* out_type_index)
{
	for (UInt32 i = 0; i < memory_properties.memoryTypeCount && type_bits; i++)
	{
		if ((type_bits & 1) == 1)
		{
			if ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties && exclude_type_index != i)
			{
				*out_type_index = i;
				return VK_SUCCESS;
			}
		}
		type_bits >>= 1;
	}
	return VK_ERROR_FEATURE_NOT_PRESENT;
}

Int VK_DeviceMemoryManager::FreeInternal(VK_DeviceMemoryAllocation*& allocation)
{
	vkFreeMemory(device->GetDevice(),allocation->GetMemory(),VULKAN_CPU_ALLOCATOR);
	--num_allocations;

	UInt32 heap_index = memory_properties.memoryTypes[allocation->property.memory_type_index].heapIndex;
	UInt32 erase_index = -1;
	memory_heaps[heap_index].used_size -= allocation->size;
	auto it = std::find(memory_heaps[heap_index].allocations.begin(), memory_heaps[heap_index].allocations.end(), allocation);
	allocation->property.is_freed_by_system = true;
	delete allocation;
	allocation = nullptr;
	if (it != memory_heaps[heap_index].allocations.end())
	{
		auto erase_it= memory_heaps[heap_index].allocations.erase(it);
		erase_index = std::distance(memory_heaps[heap_index].allocations.begin(), erase_it);
	}
	return erase_index;
}

void VK_DeviceMemoryManager::TrimMemory(Bool is_full_trim)
{
/*

���������Vulkan��Ⱦ������豸�ڴ��������һ���������������ڴ治��ʱ�ͷ�һЩ�ڴ档�ú������ȶ��ڴ����б���������ÿ���ڴ�飬��������һ�������ͷŵ���ֵ��Ȼ��������ڴ���е�ÿ������顣
���һ��������Ѿ�����������һ��ָ����֡����FrameThresholdFull����������Ҫ����ȫ����ڴ��ͷţ�bFullTrim������ô�÷���齫���ͷţ���������÷�����Ѿ����������������ͷŵ���ֵ��FrameThresholdPartial����
�������AbovePartialThreshold�������ӡ����AbovePartialThreshold�����������˲����ͷŵ���ֵ��ThresholdPartial�������Ӹ��ڴ����ɾ��һЩ����飬ֱ��AbovePartialThreshold���������ڲ����ͷŵ���ֵ��
���ͷ�һЩ�ڴ档���ͷ�һ�������ʱ���ú�����������Ӧ�ڴ����͵ļ������Լ����ڴ���������ú���ʵ�������ڴ治��ʱ�ͷ��ڴ�Ĺ��ܣ����ҿ������ڴ��Ĵ�С�ͱ���ʱ������أ����Ż��ڴ��ͷŵ�Ч�ʺ����ܡ�
*/
    //blocks are always freed after being reserved for FrameThresholdFull frames.
	CONST UInt32 frame_threshold_full = 100;
	// After being held for FrameThresholdPartial frames, only LargeThresholdPartial/SmallThresholdPartial pages are kept reserved
	// SmallPageSize defines when Large/Small is used.
	CONST UInt32 frame_threshold_partial = 10;
	CONST UInt32 large_threshold_partial = 5;
	CONST UInt32 small_threshold_partial = 10;
	CONST UInt64 small_page_size = (8llu << 20);

	UInt32 frame = g_frame_number_render_thread.load();
	for (auto& pair : memory_block_map)
	{
		MemoryBlock& block = pair.second;
		const UInt32 threshold_partial = block.key.block_size <= small_page_size ? small_threshold_partial : large_threshold_partial;

		UInt32 above_partial_threshold = 0;
		Int index = 0;
		while (index < block.allocations.size())
		{
			MemoryBlock::FreeBlock& free_block = block.allocations[index];
			if (free_block.frame_freed + frame_threshold_full < frame || is_full_trim)
			{
				VkDeviceSize allocation_size = free_block.allocation->GetSize();

				FreeInternal(free_block.allocation);
				auto it = block.allocations.begin()+ index;
				block.allocations.erase(it);
				continue;
			}
			else if (free_block.frame_freed + frame_threshold_partial < frame)
			{
				above_partial_threshold++;
			}
			index++;
		}
		if (above_partial_threshold > threshold_partial)
		{
			UInt32 blocks_to_free = above_partial_threshold - threshold_partial;
			index = 0;
			while (index < block.allocations.size() && blocks_to_free > 0)
			{
				MemoryBlock::FreeBlock& free_block = block.allocations[index];
				if (free_block.frame_freed + frame_threshold_partial < frame)
				{
					VkDeviceSize allocation_size = free_block.allocation->GetSize();

					FreeInternal(free_block.allocation);
					auto it = block.allocations.begin() + index;
					block.allocations.erase(it);
					blocks_to_free--;
					continue;
				}
				else
				{
					index++;
				}
			}
		}
	}


}

VK_MemoryManager::VK_MemoryManager(VK_Device* in_device):device_memory_manager(in_device->GetDeviceMemoryManager()),device(in_device)
{
    CONST UInt32 type_bits = (1<<device_memory_manager->GetMemoryTypeNum())-1;
    CONST VkPhysicalDeviceMemoryProperties& memory_properties = device_memory_manager->GetMemoryProperties();
    resource_heaps.resize(memory_properties.memoryTypeCount,nullptr);

	auto GetMemoryTypesFromPropertiesFunc = [memory_properties](UInt32 in_type_bits, VkMemoryPropertyFlags properties, Vector<UInt32>& out_type_indices)
	{
		for (UInt32 i = 0; i < memory_properties.memoryTypeCount && in_type_bits; ++i)
		{
			if ((in_type_bits & 1) == 1)
			{
				// Type is available, does it match user properties?
				if ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					out_type_indices.push_back(i);
				}
			}
			in_type_bits >>= 1;
		}
		return out_type_indices.size() > 0;
	};

    {
        UInt32 type_index=0;
        device_memory_manager->GetMemoryTypeFromProperties(type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &type_index);
        UInt64 heap_size =memory_properties.memoryHeaps[memory_properties.memoryTypes[type_index].heapIndex].size;
        resource_heaps[type_index]= new VK_MemoryResourceHeap(this,type_index, STAGING_HEAP_PAGE_SIZE);
        auto& page_size_buckets= resource_heaps[type_index]->page_size_buckets;
        VK_VulkanPageSizeBucket bucket0 = {STAGING_HEAP_PAGE_SIZE,STAGING_HEAP_PAGE_SIZE,VK_VulkanPageSizeBucket::BUCKET_MASK_IMAGE|VK_VulkanPageSizeBucket::BUCKET_MASK_BUFFER};
        VK_VulkanPageSizeBucket bucket1= { UINT64_MAX, 0, VK_VulkanPageSizeBucket::BUCKET_MASK_IMAGE | VK_VulkanPageSizeBucket::BUCKET_MASK_BUFFER };
        page_size_buckets.push_back(bucket0);
        page_size_buckets.push_back(bucket1);
    }

    {
        UInt32 type_index=0;
        UInt32 host_vis_cached_index=0;
        VkResult host_cache_result= device_memory_manager->GetMemoryTypeFromProperties(type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, &host_vis_cached_index);
        UInt32 host_vis_index=0;
        VkResult host_result = device_memory_manager->GetMemoryTypeFromProperties(type_bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &host_vis_index);
		if (host_cache_result == VK_SUCCESS)
		{
			type_index = host_vis_cached_index;
		}
		else if (host_result == VK_SUCCESS)
		{
            type_index = host_vis_index;
		}
        CHECK_WITH_LOG(host_cache_result != VK_SUCCESS&& host_result != VK_SUCCESS,"RHI Error : No Memory Type Found Supporting VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ! ")
		UInt64 heap_size = memory_properties.memoryHeaps[memory_properties.memoryTypes[type_index].heapIndex].size;
		if(!resource_heaps[type_index])
			resource_heaps[type_index] = new VK_MemoryResourceHeap(this, type_index, STAGING_HEAP_PAGE_SIZE);
		auto& page_size_buckets= resource_heaps[type_index]->page_size_buckets;
		VK_VulkanPageSizeBucket bucket0 = { STAGING_HEAP_PAGE_SIZE, STAGING_HEAP_PAGE_SIZE, VK_VulkanPageSizeBucket::BUCKET_MASK_IMAGE | VK_VulkanPageSizeBucket::BUCKET_MASK_BUFFER };
        VK_VulkanPageSizeBucket bucket1 = { UINT64_MAX, 0, VK_VulkanPageSizeBucket::BUCKET_MASK_IMAGE | VK_VulkanPageSizeBucket::BUCKET_MASK_BUFFER };
        page_size_buckets.push_back(bucket0);
        page_size_buckets.push_back(bucket1);
    }

	{
		Vector<UInt32> type_indices;
		GetMemoryTypesFromPropertiesFunc(type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, type_indices);

		for (UInt32 index = 0; index < memory_properties.memoryTypeCount; ++index)
		{
			Int heap_index = memory_properties.memoryTypes[index].heapIndex;
			VkDeviceSize heap_size = memory_properties.memoryHeaps[heap_index].size;
			if (!resource_heaps[index])
			{
                resource_heaps[index] = new VK_MemoryResourceHeap(this, index);
                resource_heaps[index]->is_support_host_cached = ((memory_properties.memoryTypes[index].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
                resource_heaps[index]->is_support_lazily_allocated = ((memory_properties.memoryTypes[index].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) == VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
				auto& page_size_buckets = resource_heaps[index]->page_size_buckets;

#if PLATFORM_ANDROID
				VK_VulkanPageSizeBucket BucketImage = { UINT64_MAX, (UInt32)ANDROID_MAX_HEAP_IMAGE_PAGE_SIZE, VK_VulkanPageSizeBucket::BUCKET_MASK_IMAGE };
				VK_VulkanPageSizeBucket BucketBuffer = { UINT64_MAX, (UInt32)ANDROID_MAX_HEAP_BUFFER_PAGE_SIZE, VK_VulkanPageSizeBucket::BUCKET_MASK_BUFFER };
				page_size_buckets.push_back(BucketImage);
				page_size_buckets.push_back(BucketBuffer);
#else
				UInt32 small_allocation_threshold = 2 << 20;
                UInt32 large_allocation_threshold = VK_MEMORY_MAX_SUB_ALLOCATION;
				VkDeviceSize small_page_size = 8llu << 20;
				VkDeviceSize large_page_size = std::min<VkDeviceSize>(heap_size / 8, GPU_ONLY_HEAP_PAGE_SIZE);

				VK_VulkanPageSizeBucket bucket_small_image = { small_allocation_threshold, (UInt32)small_page_size, VK_VulkanPageSizeBucket::BUCKET_MASK_IMAGE };
                VK_VulkanPageSizeBucket bucket_large_image = { large_allocation_threshold, (UInt32)large_page_size, VK_VulkanPageSizeBucket::BUCKET_MASK_IMAGE };
                VK_VulkanPageSizeBucket bucket_small_buffer = { small_allocation_threshold, (UInt32)small_page_size, VK_VulkanPageSizeBucket::BUCKET_MASK_BUFFER };
                VK_VulkanPageSizeBucket bucket_large_buffer = { large_allocation_threshold, (UInt32)large_page_size, VK_VulkanPageSizeBucket::BUCKET_MASK_BUFFER };
                VK_VulkanPageSizeBucket bucket_remainder = { UINT64_MAX, 0, VK_VulkanPageSizeBucket::BUCKET_MASK_BUFFER | VK_VulkanPageSizeBucket::BUCKET_MASK_IMAGE };
                page_size_buckets.push_back(bucket_small_image);
                page_size_buckets.push_back(bucket_large_image);
                page_size_buckets.push_back(bucket_small_buffer);
                page_size_buckets.push_back(bucket_large_buffer);
                page_size_buckets.push_back(bucket_remainder);
#endif
			}
		}
	}

}


VK_DeviceMemoryManager* VK_MemoryManager::GetDeviceMemoryManager()
{
    return device_memory_manager;
}
Bool VK_MemoryManager::AllocateBufferPooled(VK_Allocation& out_allocation, UInt32 in_size, UInt32 in_min_alignment, VkBufferUsageFlags in_buffer_usage_flags, VkMemoryPropertyFlags in_memory_property_flags, ENUM_VK_AllocationMetaType in_meta_type)
{

	UInt32 alignment = std::max(in_min_alignment, CalculateBufferAlignmentFromVKUsageFlags(device, in_buffer_usage_flags));
	CONST Float32 Priority = CalculateBufferPriority(in_buffer_usage_flags);

	CONST Int pool_size = (Int)GetPoolTypeForAlloc(in_size, alignment);
	if (pool_size != (Int)EPoolSizes::SizesCount)
	{
		in_size = PoolSizes[pool_size];
	}

	for (Int index = 0; index < used_buffer_allocations[pool_size].size(); ++index)
	{
		VK_MemoryResourceFragmentAllocator* subresource_allocator = used_buffer_allocations[pool_size][index];
		if ((subresource_allocator->buffer_usage_flags & in_buffer_usage_flags) == in_buffer_usage_flags &&
			(subresource_allocator->memory_property_flags & in_memory_property_flags) == in_memory_property_flags)
		{

			if (subresource_allocator->TryAllocate(out_allocation, nullptr, in_size, alignment, in_meta_type))
			{
				return true;
			}
		}
	}

	for (Int index = 0; index < free_buffer_allocations[pool_size].size(); ++index)
	{
		VK_MemoryResourceFragmentAllocator* subresource_allocator = free_buffer_allocations[pool_size][index];
		if ((subresource_allocator->buffer_usage_flags & in_buffer_usage_flags) == in_buffer_usage_flags &&
			(subresource_allocator->memory_property_flags & in_memory_property_flags) == in_memory_property_flags)
		{

			if (subresource_allocator->TryAllocate(out_allocation, nullptr, in_size, alignment, in_meta_type))
			{
				used_buffer_allocations[pool_size].push_back(subresource_allocator);
				auto it = free_buffer_allocations[pool_size].begin() + index;
				free_buffer_allocations[pool_size].erase(it);
				return true;
			}
		}
	}

	// New Buffer
	CONST UInt32 buffer_size = std::max(in_size, BufferSizes[pool_size]);

	VkBuffer buffer;
	VkBufferCreateInfo buffer_create_info{};
	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.size = buffer_size;
	buffer_create_info.usage = in_buffer_usage_flags | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	// For descriptors buffers
	if (device->GetOptionalExtensions().HasBufferDeviceAddress)
	{
		buffer_create_info.usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	}
	vkCreateBuffer(device->GetDevice(), &buffer_create_info, VULKAN_CPU_ALLOCATOR, &buffer);

	VkMemoryRequirements MemReqs;
	vkGetBufferMemoryRequirements(device->GetDevice(), buffer, &MemReqs);
	alignment = std::max((UInt32)MemReqs.alignment, alignment);
	CHECK(MemReqs.size < buffer_size);

	UInt32 memory_type_index;
	device->GetDeviceMemoryManager()->GetMemoryTypeFromProperties(MemReqs.memoryTypeBits, in_memory_property_flags, &memory_type_index);

	bool is_has_unified_memory = device_memory_manager->HasUnifiedMemory();
	VK_DeviceMemoryAllocation* device_memory_allocation = device_memory_manager->Alloc(MemReqs.size, memory_type_index, nullptr, Priority, false);
	if (!device_memory_allocation)
	{
		in_memory_property_flags &= (~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CONST UInt32 forbidden_memory_type_index = memory_type_index;
		if ( VK_SUCCESS == device->GetDeviceMemoryManager()->GetMemoryTypeFromPropertiesExcluding(MemReqs.memoryTypeBits, in_memory_property_flags, forbidden_memory_type_index, &memory_type_index))
		{
			device_memory_allocation = device_memory_manager->Alloc( MemReqs.size, memory_type_index, nullptr, Priority, false);
		}
	}

	vkBindBufferMemory(device->GetDevice(), buffer, device_memory_allocation->GetMemory(), 0);
	UInt8 allocation_flags = 0;
	if (!is_has_unified_memory && MetaTypeCanEvict(in_meta_type))
	{
		allocation_flags |= VulkanAllocationFlagsCanEvict;
	}
	// Map host-visible pages up front so sub-allocations get a persistent
	// pointer (CheckIsMapped() only reports an EXISTING mapping - fresh pages
	// are never mapped yet, which left GetMappedPointer() returning null).
	if (device_memory_allocation->GetIsCanBeMapped())
	{
		device_memory_allocation->Map(buffer_size, 0);
	}

	Int BufferId = 0;

	VK_MemoryResourceFragmentAllocator* subresource_allocator = new VK_MemoryResourceFragmentAllocator(ENUM_VK_AllocationType::EVulkanAllocationPooledBuffer, this, allocation_flags, device_memory_allocation, memory_type_index,
		in_memory_property_flags, MemReqs.alignment, buffer, buffer_size, BufferId, in_buffer_usage_flags, pool_size);

	RegisterSubresourceAllocator(subresource_allocator);
	used_buffer_allocations[pool_size].push_back(subresource_allocator);

	if (subresource_allocator->TryAllocate(out_allocation,nullptr, in_size,alignment, in_meta_type))
	{
		return true;
	}
	return false;

}
Bool VK_MemoryManager::AllocateImageMemory(VK_Allocation& out_allocation, VkImage in_image, ENUM_VulkanAllocationFlags in_alloc_flags, UInt32 in_force_min_alignment /*= 1*/)
{

	VkMemoryRequirements memory_requirements;
	vkGetImageMemoryRequirements( device->GetDevice(), in_image, &memory_requirements);
	// Allow caller to force his own alignment requirements
	memory_requirements.alignment = std::max(memory_requirements.alignment, (VkDeviceSize)in_force_min_alignment);


	// For now, translate all the flags into a call to the legacy AllocateImageMemory() function
	CONST VkMemoryPropertyFlags memory_property_flags = InternalGetMemoryPropertyFlags(in_alloc_flags, device_memory_manager->GetIsSupportUnifiedMemory());
	CONST Bool is_external = EnumHasAllFlags(in_alloc_flags, ENUM_VulkanAllocationFlags::External);
	CONST Bool is_force_separate_allocation = EnumHasAllFlags(in_alloc_flags, ENUM_VulkanAllocationFlags::Dedicated);

	//AllocateImageMemory
	{
		VkMemoryPropertyFlags use_memory_property_flags = memory_property_flags;
		UInt32 type_index = 0;
		VkResult result = device_memory_manager->GetMemoryTypeFromProperties(memory_requirements.memoryTypeBits, use_memory_property_flags, &type_index);
		Bool is_mapped = VKHasAllFlags(use_memory_property_flags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		if ((result != VK_SUCCESS) || !resource_heaps[type_index])
		{
			if (VKHasAllFlags(memory_property_flags, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT))
			{
				// Try non-lazy flag
				use_memory_property_flags = use_memory_property_flags & ~VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
				device_memory_manager->GetMemoryTypeFromProperties(memory_requirements.memoryTypeBits, use_memory_property_flags, &type_index);
			}
		}

		CONST Bool is_force_separate_allocation = is_external || VKHasAllFlags(use_memory_property_flags, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);
		if (!resource_heaps[type_index]->AllocateResource(out_allocation, nullptr, ENUM_VK_HeapAllocationType::Image, memory_requirements.size, memory_requirements.alignment, is_mapped, is_force_separate_allocation, ENUM_VK_AllocationMetaType::EVulkanAllocationMetaImageOther, is_external))
		{

			use_memory_property_flags &= (~VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			CHECK_WITH_LOG(device_memory_manager->GetMemoryTypeFromPropertiesExcluding(memory_requirements.memoryTypeBits, use_memory_property_flags, type_index, &type_index) != VK_SUCCESS,
				"RHI Error: failed to getMemoryTypeFromPropertiesExcluding index !")
				is_mapped = VKHasAllFlags(use_memory_property_flags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
			if (!resource_heaps[type_index]->AllocateResource(out_allocation, nullptr, ENUM_VK_HeapAllocationType::Image, memory_requirements.size, memory_requirements.alignment, is_mapped, is_force_separate_allocation, ENUM_VK_AllocationMetaType::EVulkanAllocationMetaImageOther, is_external))
			{
				return false;
			}
		}
		return true;
	}
}

Bool VK_MemoryManager::AllocateBufferMemory(VK_Allocation& out_allocation, VkBuffer in_buffer, ENUM_VulkanAllocationFlags in_alloc_flags, UInt32 in_force_min_alignment /*= 1*/)
{
	VkBufferMemoryRequirementsInfo2 buffer_memory_requirements_Info;
    buffer_memory_requirements_Info.sType= VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
	buffer_memory_requirements_Info.buffer = in_buffer;
	buffer_memory_requirements_Info.pNext = nullptr;
	VkMemoryDedicatedRequirements dedicated_requirements;
    dedicated_requirements.sType= VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
	dedicated_requirements.pNext = nullptr;
	VkMemoryRequirements2 memory_requirements;
    memory_requirements.sType= VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
	memory_requirements.pNext = &dedicated_requirements;

	vkGetBufferMemoryRequirements2(device ->GetDevice(), &buffer_memory_requirements_Info, &memory_requirements);

	// Allow caller to force his own alignment requirements
	memory_requirements.memoryRequirements.alignment = std::max(memory_requirements.memoryRequirements.alignment, (VkDeviceSize)in_force_min_alignment);

	if (dedicated_requirements.requiresDedicatedAllocation || dedicated_requirements.prefersDedicatedAllocation)
	{
		in_alloc_flags |= ENUM_VulkanAllocationFlags::Dedicated;
	}

	// For now, translate all the flags into a call to the legacy AllocateBufferMemory() function
	CONST VkMemoryPropertyFlags memory_property_flags = InternalGetMemoryPropertyFlags(in_alloc_flags, device_memory_manager->GetIsSupportUnifiedMemory());
	CONST Bool is_external = EnumHasAllFlags(in_alloc_flags, ENUM_VulkanAllocationFlags::External);
	CONST Bool is_force_separate_allocation = EnumHasAllFlags(in_alloc_flags, ENUM_VulkanAllocationFlags::Dedicated);

    //AllocateBufferMemory
    {
        VkMemoryPropertyFlags use_memory_property_flags = memory_property_flags;
		UInt32 type_index = 0;
		VkResult result = device_memory_manager->GetMemoryTypeFromProperties(memory_requirements.memoryRequirements.memoryTypeBits, use_memory_property_flags, &type_index);
		Bool is_mapped = VKHasAllFlags(use_memory_property_flags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		if ((result != VK_SUCCESS) || !resource_heaps[type_index])
		{
			if (VKHasAllFlags(use_memory_property_flags, VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
			{
				// Try non-cached flag
                use_memory_property_flags = use_memory_property_flags & ~VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
			}

			if (VKHasAllFlags(memory_property_flags, VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT))
			{
				// Try non-lazy flag
                use_memory_property_flags = use_memory_property_flags & ~VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
			}

			// Try another heap type
			UInt32 original_type_index = type_index;
			if (device_memory_manager->GetMemoryTypeFromPropertiesExcluding(memory_requirements.memoryRequirements.memoryTypeBits, use_memory_property_flags, (result == VK_SUCCESS) ? type_index : (UInt32)-1, &type_index) != VK_SUCCESS)
			{

			}
			if (!resource_heaps[type_index])
			{
			}
		}

		//check(MemoryReqs.size <= (uint64)MAX_uint32);

		if (!resource_heaps[type_index]->AllocateResource(out_allocation, nullptr, ENUM_VK_HeapAllocationType::Buffer, memory_requirements.memoryRequirements.size, memory_requirements.memoryRequirements.alignment, is_mapped, is_force_separate_allocation, ENUM_VK_AllocationMetaType::EVulkanAllocationMetaBufferOther, is_external))
		{
			// Try another memory type if the allocation failed
			CHECK_WITH_LOG(device_memory_manager->GetMemoryTypeFromPropertiesExcluding(memory_requirements.memoryRequirements.memoryTypeBits, memory_property_flags, type_index, &type_index)!= VK_SUCCESS,
                "RHI Error: failed to getMemoryTypeFromPropertiesExcluding index !")
            is_mapped = (memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
			if (resource_heaps[type_index])
            { 
                if (!(resource_heaps[type_index]->AllocateResource(out_allocation, nullptr, ENUM_VK_HeapAllocationType::Buffer, memory_requirements.memoryRequirements.size, memory_requirements.memoryRequirements.alignment, is_mapped, is_force_separate_allocation, ENUM_VK_AllocationMetaType::EVulkanAllocationMetaBufferOther, is_external)))
                {
                    return false;
                }
            }
		}
		return true;
    }

	if (out_allocation.IsValid())
	{
		if (EnumHasAllFlags(in_alloc_flags, ENUM_VulkanAllocationFlags::AutoBind))
		{
			VkBindBufferMemoryInfo bind_buffer_memory_info;
            bind_buffer_memory_info.sType= VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO;
			bind_buffer_memory_info.buffer = in_buffer;
			bind_buffer_memory_info.memory = out_allocation.GetDeviceMemoryHandle(device);
			bind_buffer_memory_info.memoryOffset = out_allocation.offset;

			CHECK_WITH_LOG(vkBindBufferMemory2(device->GetDevice(), 1, &bind_buffer_memory_info)!= VK_SUCCESS,
                            "RHI Error : failed to bind buffer memory !")
		}

	}
	else
	{
		if (!EnumHasAllFlags(in_alloc_flags, ENUM_VulkanAllocationFlags::NoError))
		{
			CONST Bool IsHostMemory = EnumHasAnyFlags(in_alloc_flags, ENUM_VulkanAllocationFlags::HostVisible | ENUM_VulkanAllocationFlags::HostCached);
			//HandleOOM(false, IsHostMemory ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_ERROR_OUT_OF_DEVICE_MEMORY, MemoryRequirements.memoryRequirements.size);
		}
	}

	return out_allocation.IsValid();
}

VK_MemoryResourceHeap::VK_MemoryResourceHeap(VK_MemoryManager* in_owner, UInt32 in_memory_type_index, UInt32 in_override_page_size /*= 0*/)
: owner(in_owner),memory_type_index(in_memory_type_index),override_page_size(in_override_page_size)
{
    heap_index=owner->GetDeviceMemoryManager()->GetHeapIndex(memory_type_index);
}

VK_MemoryResourceHeap::~VK_MemoryResourceHeap()
{
	auto delete_pages_func = [&](Vector<VK_MemoryResourceFragmentAllocator*>& used_pages)->Bool
		{
			bool is_leak = false;
			for (Int index = used_pages.size() - 1; index >= 0; --index)
			{
				VK_MemoryResourceFragmentAllocator* page = used_pages[index];
				is_leak |= !page->MergeFreeBlocks();
				owner->GetDeviceMemoryManager()->Free(page->memory_allocation);
				delete page;
			}
			used_pages.clear();
			return is_leak;
		};
	for (Vector<VK_MemoryResourceFragmentAllocator*>& pages : active_pages)
	{
		delete_pages_func(pages);
	}
}

Bool VK_MemoryResourceHeap::AllocateResource(VK_Allocation& out_allocation, VK_Evictable* allocation_owner, ENUM_VK_HeapAllocationType type, UInt32 size, UInt32 alignment, Bool is_map_allocation, Bool is_force_separate_allocation, ENUM_VK_AllocationMetaType meta_type, Bool is_external)
{

    VK_DeviceMemoryManager* device_memory_manager = owner->GetDeviceMemoryManager();
    VK_VulkanPageSizeBucket memory_bucket;
    UInt32 bucket_id= GetPageSizeBucket(memory_bucket,type,size , is_force_separate_allocation);
    Bool is_support_unified_memory= device_memory_manager->GetIsSupportUnifiedMemory();
    Vector<VK_MemoryResourceFragmentAllocator*>& used_pages = active_pages[bucket_id];
    ENUM_VK_AllocationType allocation_type = (type==ENUM_VK_HeapAllocationType::Image)? ENUM_VK_AllocationType::EVulkanAllocationImage : 
                                             (type == ENUM_VK_HeapAllocationType::Buffer) ? ENUM_VK_AllocationType::EVulkanAllocationBuffer: ENUM_VK_AllocationType::EVulkanAllocationEmpty;
    UInt8 allocation_flags = (!is_support_unified_memory&& MetaTypeCanEvict(meta_type))? VulkanAllocationFlagsCanEvict: 0;
    if(is_map_allocation)
    {
        allocation_flags |= VulkanAllocationFlagsMapped;
    }
    UInt32 allocation_size;
    if (is_force_separate_allocation == false)
    {
        if (size < memory_bucket.page_size)
        {
            for (Int index = 0 ; index < used_pages.size(); ++index)
            {
                VK_MemoryResourceFragmentAllocator* page = used_pages[index];
                if (page->GetSubresourceAllcatorFlags()== allocation_flags)
                {   
					if (page->TryAllocate(out_allocation,allocation_owner,size,alignment,meta_type))
					{
	                    return true;
					}
                }
            }
        }
		allocation_size = std::max(size,memory_bucket.page_size);
    }
	else
	{
		allocation_size = size;
	}

	VK_DeviceMemoryAllocation* device_memory_allocation = device_memory_manager->Alloc( allocation_size, memory_type_index, nullptr, VULKAN_MEMORY_HIGHEST_PRIORITY, is_external);
	if (!device_memory_allocation && size != allocation_size)
	{
		// Retry with a smaller size
		device_memory_allocation = device_memory_manager->Alloc( size, memory_type_index, nullptr, VULKAN_MEMORY_HIGHEST_PRIORITY, is_external);
		if (!device_memory_allocation)
		{
			return false;
		}
	}
	if (!device_memory_allocation)
	{
		//LOG
	}
	if (is_map_allocation)
	{
		device_memory_allocation->Map(allocation_size, 0);
	}

	UInt32 buffer_id = 0;
	//if (UseVulkanDescriptorCache())
	//{
	//	BufferId = ++GVulkanBufferHandleIdCounter;
	//}
	++page_id_counter;
	VK_MemoryResourceFragmentAllocator* Page = new VK_MemoryResourceFragmentAllocator(allocation_type,owner, allocation_flags, device_memory_allocation, memory_type_index, buffer_id);
	owner->RegisterSubresourceAllocator(Page);
	Page->bucket_id = bucket_id;
	Page->alignment = alignment;
	active_pages[bucket_id].push_back(Page);

	used_memory += allocation_size;

	peak_page_size = std::max(peak_page_size, allocation_size);


	Bool bOk = Page->TryAllocate(out_allocation, allocation_owner, size, alignment, meta_type);
    return bOk;
}

void VK_MemoryResourceHeap::ReleasePage(VK_MemoryResourceFragmentAllocator* in_page)
{
	owner->UnregisterSubresourceAllocator(in_page);
    VK_DeviceMemoryAllocation* allocation = in_page->memory_allocation;
	in_page->memory_allocation = 0;
	owner->GetDeviceMemoryManager()->Free(allocation);
    used_memory -= in_page->max_size;
	delete in_page;
}

void VK_MemoryResourceHeap::FreePage(VK_MemoryResourceFragmentAllocator* in_page)
{

	//in_page->FrameFreed = GFrameNumberRenderThread;

	if (in_page->type == ENUM_VK_AllocationType::EVulkanAllocationImageDedicated)
	{
		auto it = std::find(used_dedicated_pages.begin(), used_dedicated_pages.end(), in_page);
		if (it !=used_dedicated_pages.end())
		{
			used_dedicated_pages.erase(it);
		}
		ReleasePage(in_page);

	}
	else
	{
		UInt8 bucket_id = in_page->bucket_id;

		Vector<VK_MemoryResourceFragmentAllocator*>& pages = active_pages[bucket_id];
		auto it = std::find(pages.begin(), pages.end(), in_page);
		if (it != pages.end())
		{
			pages.erase(it);
		}
		ReleasePage(in_page);
	}
}

UInt32 VK_MemoryResourceHeap::GetPageSizeBucket(VK_VulkanPageSizeBucket& out_bucket, ENUM_VK_HeapAllocationType type, UInt32 allocation_size, Bool is_force_single_allocation)
{
    if (is_force_single_allocation)
    {
        UInt32 bucket_id=page_size_buckets.size()-1;
        out_bucket=page_size_buckets[bucket_id];
        return bucket_id;
    }
    UInt32 mask=0;
    mask |= type== ENUM_VK_HeapAllocationType::Image? VK_VulkanPageSizeBucket::BUCKET_MASK_IMAGE : 0;
    mask |= type == ENUM_VK_HeapAllocationType::Buffer ? VK_VulkanPageSizeBucket::BUCKET_MASK_BUFFER : 0;
    for (VK_VulkanPageSizeBucket& bucket : page_size_buckets)
    {
        if (mask == (bucket.bucket_mask & mask) && allocation_size <= bucket.max_allocation)
        {
            out_bucket=bucket;
            return &bucket - &page_size_buckets[0];
        }
    }

    return 0xffffffff;
}

Bool VK_MemoryResourceFragmentAllocator::TryAllocate(VK_Allocation& out_allocation, VK_Evictable* allocation_owner, UInt32 in_size, UInt32 in_alignment, ENUM_VK_AllocationMetaType in_meta_type)
{
    if (is_evicting || is_locked)
    {
        return false;
    }
	in_alignment = std::max(in_alignment, alignment);
	for (Int index = 0; index < free_list.size(); ++index)
	{
        VK_Section& entry = free_list[index];
		UInt32 allocated_offset = entry.offset;
        UInt32 aligned_offset = Align(entry.offset, in_alignment);
        UInt32 alignment_adjustment = aligned_offset - entry.offset;
        UInt32 allocated_size = alignment_adjustment + in_size;
		if (allocated_size <= entry.size)
		{
            VK_Section::AllocateFromEntry(free_list, index, allocated_size);

			used_size += allocated_size;
			Int extra_offset = AllocateInternalData();
			out_allocation.Init(type, in_meta_type, (UInt64)buffer, in_size, aligned_offset, allocator_index, extra_offset, buffer_id);
			memory_used[(UInt32)in_meta_type] += allocated_size;
			static UInt32 uid_counter = 0;
			uid_counter++;
			internal_data[extra_offset].Init(out_allocation, allocation_owner, allocated_offset, allocated_size, in_alignment);
			alloc_calls++;
			num_sub_allocations++;

			// Register back-reference for defrag
			if (allocation_refs.size() <= (size_t)extra_offset)
				allocation_refs.resize(extra_offset + 1, nullptr);
			allocation_refs[extra_offset] = &out_allocation;

			is_defragging = false;
			return true;
		}
	}
	return false;
}

UInt8 VK_MemoryResourceFragmentAllocator::GetSubresourceAllcatorFlags()
{
    return subresource_allocator_flags;
}
void VK_MemoryResourceFragmentAllocator::SetAllocationOwner(UInt32 alloc_index, VK_Evictable* owner)
{
	internal_data[alloc_index].allocation_owner = owner;
}

void VK_MemoryResourceFragmentAllocator::Free(VK_Allocation& allocation)
{
	Bool bTryFree = false;

	{
		free_calls++;
		UInt32 allocation_offset;
        UInt32 allocation_size;
		{
			// [AI] Defense-in-depth: verify allocation_index is in bounds for this page
			// before indexing internal_data. A stale index (from page-index aliasing) would
			// otherwise dereference a wrong page's array and crash at the state check below.
			if (allocation.allocation_index < 0 || allocation.allocation_index >= (Int)internal_data.size())
			{
				CHECK_WITH_LOG_WARNING(true, "RHI Warning: allocation_index out of bounds in Free - possible page-index aliasing, skipping free");
				allocation.allocation_index = -1;
				return;
			}
			VK_AllocationInternalInfo& data = internal_data[allocation.allocation_index];
			Bool is_was_discarded = data.state == VK_AllocationInternalInfo::ENUM_VK_AllocationState::EFREEDISCARDED;
			CHECK_WITH_LOG(!(data.state == VK_AllocationInternalInfo::ENUM_VK_AllocationState::EALLOCATED || data.state == VK_AllocationInternalInfo::ENUM_VK_AllocationState::EFREEPENDING || data.state == VK_AllocationInternalInfo::ENUM_VK_AllocationState::EFREEDISCARDED),
                "RHI Error : failed to free vk_allocation because data.state error!");
			allocation_offset = data.allocation_offset;
			allocation_size = data.allocation_size;
			if (!is_was_discarded)
			{
				memory_used[(UInt32)allocation.meta_type] -= allocation_size;
			}
			data.state = VK_AllocationInternalInfo::ENUM_VK_AllocationState::EFREED;
			if (allocation.allocation_index < (Int)allocation_refs.size())
				allocation_refs[allocation.allocation_index] = nullptr;
			FreeInternalData(allocation.allocation_index);
            allocation.allocation_index = -1;
			if (is_was_discarded)
			{
				//this occurs if we do full defrag when there are pending frees. in that case the memory is just not moved to the new block.
				return;
			}
		}
		VK_Section new_free;
		new_free.offset = allocation_offset;
		new_free.size = allocation_size;
		//check(NewFree.Offset <= GetMaxSize());
		//check(NewFree.Offset + NewFree.Size <= GetMaxSize());
        VK_Section::Add(free_list, new_free);
		used_size -= allocation_size;
		num_sub_allocations--;
		//check(UsedSize >= 0);
		if (MergeFreeBlocks())
		{
			bTryFree = true; //cannot free here as it will cause incorrect lock ordering
		}
	}

	if (bTryFree)
	{
		owner_memory_manager->ReleaseSubresourceAllocator(this);
	}
}

void VK_MemoryManager::ReleaseSubresourceAllocator(VK_MemoryResourceFragmentAllocator* subresource_allocator)
{
	if (subresource_allocator->type == ENUM_VK_AllocationType::EVulkanAllocationPooledBuffer)
	{
		auto it = std::find(used_buffer_allocations[subresource_allocator->pool_size_index].begin(), used_buffer_allocations[subresource_allocator->pool_size_index].end(), subresource_allocator);
		if (it != used_buffer_allocations[subresource_allocator->pool_size_index].end())
		{
			used_buffer_allocations[subresource_allocator->pool_size_index].erase(it);
		}
		// [AI] Restored: mark when the page was freed so ReleaseFreedResources
		// can enforce the 3-frame grace period before releasing to OS.
		subresource_allocator->frame_freed = g_frame_number_render_thread.load();
		free_buffer_allocations[subresource_allocator->pool_size_index].push_back(subresource_allocator);
	}
	else
	{
		VK_MemoryResourceHeap* heap = resource_heaps[subresource_allocator->memory_type_index];
		heap->FreePage(subresource_allocator);
	}
}

void VK_MemoryManager::FreeAllocation(VK_Allocation& allocation)
{
	CONST UInt32 index = allocation.allocator_index;
	GetSubresourceAllocator(index)->Free(allocation);
}

VK_MemoryResourceFragmentAllocator* VK_MemoryManager::GetSubresourceAllocator(CONST UInt32 allocator_index)
{
	return all_buffer_allocations[allocator_index];
}

void VK_MemoryManager::RegisterSubresourceAllocator(VK_MemoryResourceFragmentAllocator* subresource_allocator)
{
	// [AI] Free-list recycle: each freed index is reused at most once, preventing
	// page-index aliasing when multiple pages are unregistered between registrations.
	if (!free_allocator_indices.empty())
	{
		CONST UInt32 index = free_allocator_indices.back();
		free_allocator_indices.pop_back();
		// [AI] Assert the slot is actually free before reusing it.
		CHECK_WITH_LOG(all_buffer_allocations[index] != nullptr,
			"RHI Error: page-index aliasing in RegisterSubresourceAllocator - slot was not freed");
		subresource_allocator->allocator_index = index;
		all_buffer_allocations[index] = subresource_allocator;
	}
	else
	{
		subresource_allocator->allocator_index = all_buffer_allocations.size();
		all_buffer_allocations.push_back(subresource_allocator);
	}
}

void VK_MemoryManager::UnregisterSubresourceAllocator(VK_MemoryResourceFragmentAllocator* subresource_allocator)
{
	CONST UInt32 index = subresource_allocator->allocator_index;
	all_buffer_allocations[index] = nullptr;
	// [AI] Push to free-list instead of arming a single-slot field.
	// Each index is reused at most once, preventing page-index aliasing.
	free_allocator_indices.push_back(index);
}

void VK_MemoryManager::Destroy()
{
	DestroyResourceAllocations();
	for (VK_MemoryResourceHeap* heap : resource_heaps)
	{
		delete heap;
	}
	resource_heaps.clear();
}
// ============================================================
// Defrag implementation
// ============================================================

Bool VK_MemoryResourceFragmentAllocator::CanDefrag() CONST
{
	if (is_locked || is_evicting || is_defragging) return false;
	if (num_sub_allocations == 0) return false;
	// Only pooled buffer pages can be defragged (they have a valid backing VkBuffer + usage flags).
	// Image pages and non-pooled buffer pages need vkCmdCopyImage / individual buffer moves (V2).
	if (type != ENUM_VK_AllocationType::EVulkanAllocationPooledBuffer) return false;
	if (buffer == VK_NULL_HANDLE || buffer_usage_flags == 0) return false;
	return true;
}

Bool VK_MemoryResourceHeap::TryRealloc(VK_Allocation& out_allocation, VK_Evictable* allocation_owner,
	ENUM_VK_HeapAllocationType type, UInt32 size, UInt32 alignment, ENUM_VK_AllocationMetaType meta_type)
{
	VK_VulkanPageSizeBucket bucket;
	UInt32 bucket_id = GetPageSizeBucket(bucket, type, size, false);
	if (bucket_id >= MAX_BUCKETS) return false;

	for (VK_MemoryResourceFragmentAllocator* page : active_pages[bucket_id])
	{
		if (page->is_locked || page->is_evicting || page->is_defragging)
			continue;
		if (page->TryAllocate(out_allocation, allocation_owner, size, alignment, meta_type))
			return true;
	}
	return false;
}

UInt32 VK_MemoryResourceFragmentAllocator::DefragTick(VK_Device& device, VK_CommandBuffer* cmd,
	VK_MemoryResourceHeap* heap, UInt32 max_count)
{
	last_defrag_frame = (UInt32)g_frame_number_render_thread.load();
	is_locked = true;

	UInt32 moved = 0;
	// Use the page's own backing buffer (all sub-allocations share this VkBuffer)
	VkBuffer old_buffer = buffer;
	if (old_buffer == VK_NULL_HANDLE) { is_locked = false; return 0; }

	// Create destination buffer with same properties
	VkBufferCreateInfo buf_info{};
	buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buf_info.size = max_size;
	buf_info.usage = buffer_usage_flags | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkBuffer new_buffer = VK_NULL_HANDLE;
	if (vkCreateBuffer(device.GetDevice(), &buf_info, VULKAN_CPU_ALLOCATOR, &new_buffer) != VK_SUCCESS)
	{
		is_locked = false;
		return 0;
	}

	// Allocate memory for new backing buffer
	VK_DeviceMemoryManager* mem_mgr = owner_memory_manager->GetDeviceMemoryManager();
	VkMemoryRequirements mem_reqs;
	vkGetBufferMemoryRequirements(device.GetDevice(), new_buffer, &mem_reqs);

	UInt32 type_index = memory_type_index;
	if (!mem_mgr->GetMemoryTypeFromProperties(mem_reqs.memoryTypeBits, memory_property_flags, &type_index))
	{
		vkDestroyBuffer(device.GetDevice(), new_buffer, VULKAN_CPU_ALLOCATOR);
		is_locked = false;
		return 0;
	}

	VK_DeviceMemoryAllocation* new_mem = mem_mgr->Alloc(max_size, type_index, nullptr, 1.0f, false);
	if (!new_mem)
	{
		vkDestroyBuffer(device.GetDevice(), new_buffer, VULKAN_CPU_ALLOCATOR);
		is_locked = false;
		return 0;
	}

	vkBindBufferMemory(device.GetDevice(), new_buffer, new_mem->GetMemory(), 0);

	// Copy live allocations
	cmd->Begin();
	UInt32 new_offset = 0;
	for (UInt32 i = 0; i < internal_data.size() && moved < max_count; ++i)
	{
		auto& info = internal_data[i];
		if (info.state != VK_AllocationInternalInfo::ENUM_VK_AllocationState::EALLOCATED)
			continue;

		// Align new offset
		UInt32 aligned = (new_offset + info.alignment - 1) & ~(info.alignment - 1);
		VkBufferCopy region{};
		region.srcOffset = info.allocation_offset;
		region.dstOffset = aligned;
		region.size = info.allocation_size;
		cmd->CopyBuffer(old_buffer, new_buffer, 1, &region);

		// Update the VK_Allocation to point to new buffer/offset
		if (i < allocation_refs.size() && allocation_refs[i])
		{
			allocation_refs[i]->vulkan_handle = (UInt64)new_buffer;
			allocation_refs[i]->offset = aligned;
		}
		// Sync internal tracking - without this Free() adds the old
		// offset back to free_list, overlapping with live allocations.
		info.allocation_offset = aligned;
		new_offset = aligned + info.allocation_size;
		++moved;
	}
	cmd->MemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT);
	cmd->End();
	device.GetQueue(ENUM_QUEUE_TYPE::TRANSFER)->Submit(cmd);

		// Swap in the new buffer — free old backing memory (deferred)
		// The old buffer/memory must stay alive until the GPU finishes the
		// transfer-queue copy submitted above (Bug D2a/D2b).
		VK_DeviceMemoryAllocation* old_mem = memory_allocation;
		memory_allocation = new_mem;
		buffer = new_buffer;
		max_size = (UInt32)new_mem->GetSize();

		DeferredDestroyVkBuffer(device.GetDevice(), old_buffer, old_mem->GetMemory());

		is_locked = false;
		return moved;
	}

void VK_MemoryResourceHeap::DefragTick(VK_Device& device, UInt32 count)
{
	if (count == 0) return;

	// Scan for candidate pages — pick the most fragmented one
	VK_MemoryResourceFragmentAllocator* best = nullptr;
	Float32 best_frag = 0.0f;

	UInt32 num_buckets = (UInt32)page_size_buckets.size();
	for (UInt32 b = 0; b < num_buckets; ++b)
	{
		if (page_size_buckets[b].max_allocation == 0) continue;
		for (auto* page : active_pages[b])
		{
			if (page->is_locked || page->is_defragging || page->is_evicting)
				continue;
			if (page->num_sub_allocations == 0)
				continue;
			if (g_frame_number_render_thread.load() - page->last_defrag_frame < 100)
				continue;
			if (!page->CanDefrag())
				continue;

			Float32 used = (Float32)page->used_size;
			Float32 max_sz = (Float32)page->max_size;
			if (max_sz == 0) continue;
			Float32 free_ratio = 1.0f - used / max_sz;
			if (free_ratio < 0.3f) continue; // not fragmented enough

			if (free_ratio > best_frag)
			{
				best_frag = free_ratio;
				best = page;
			}
		}
	}

	if (!best) return;

	// Lock and defrag
	best->is_defragging = true;
	best->is_locked = true;

	VK_CommandBuffer* cmd = owner->device->GetCommandBufferManager()->GetOrCreateCommandBuffer(ENUM_QUEUE_TYPE::TRANSFER);
	UInt32 moved = best->DefragTick(device, cmd, this, count);

	if (moved > 0)
		defrag_count_down = 3;
	else
	{
		if (defrag_count_down > 0)
			--defrag_count_down;
		if (defrag_count_down == 0)
		{
			best->is_defragging = false;
			best->is_locked = false;
		}
	}
}

void VK_MemoryResourceHeap::SetDefragging(VK_MemoryResourceFragmentAllocator* page)
{
	for (UInt32 b = 0; b < MAX_BUCKETS; ++b)
	{
		for (UInt32 i = 0; i < active_pages[b].size(); ++i)
		{
			if (active_pages[b][i] == page)
			{
				page->is_defragging = true;
				// Move to end so other pages are tried first for new allocations
				if (i < active_pages[b].size() - 1)
				{
					std::swap(active_pages[b][i], active_pages[b].back());
				}
				return;
			}
		}
	}
}

void VK_MemoryManager::DefragTick()
{
	VK_Device* vk_device = device;
	for (VK_MemoryResourceHeap* heap : resource_heaps)
	{
		if (!heap) continue;
		heap->DefragTick(*vk_device, 1);
	}
}

void VK_MemoryManager::ReleaseFreedPages()
{
	ReleaseFreedResources(false);
	device_memory_manager->TrimMemory(false);
	DefragTick();
	ProcessDeferredVkBuffers(device->GetDevice());
}

void VK_MemoryManager::DestroyResourceAllocations()
{
	ReleaseFreedResources(true);

	for (auto& used_allocations : used_buffer_allocations)
	{
		for (Int index = used_allocations.size() - 1; index >= 0; --index)
		{
			VK_MemoryResourceFragmentAllocator* buffer_allocation = used_allocations[index];
			CHECK_WITH_LOG(!buffer_allocation->MergeFreeBlocks(),
				"RHI Error : Suballocation(s) for Buffer were not all released !")

			buffer_allocation->Destroy(device);
			GetDeviceMemoryManager()->Free(buffer_allocation->memory_allocation);
			delete buffer_allocation;
		}
		used_allocations.clear();
	}

	for (auto& free_allocations : free_buffer_allocations)
	{
		for (Int index = 0; index < free_allocations.size(); ++index)
		{
			VK_MemoryResourceFragmentAllocator* buffer_allocation = free_allocations[index];
			buffer_allocation->Destroy(device);
			GetDeviceMemoryManager()->Free(buffer_allocation->memory_allocation);
			delete buffer_allocation;
		}
		free_allocations.clear();
	}
}

void VK_MemoryManager::ReleaseFreedResources(Bool is_immediately)
{
	Vector<VK_MemoryResourceFragmentAllocator*> buffer_allocations_to_release;
	{
		for (auto& free_allocations : free_buffer_allocations)
		{
			for (Int index = 0; index < free_allocations.size(); ++index)
			{
				VK_MemoryResourceFragmentAllocator* buffer_allocation = free_allocations[index];
				if (is_immediately || buffer_allocation->frame_freed + VK_NUM_FRAMES_TO_WAIT_BEFORE_RELEASING_TO_OS < g_frame_number_render_thread.load())
				{
					buffer_allocations_to_release.push_back(buffer_allocation);
					auto it = free_allocations.begin() + index;
					free_allocations.erase(it);
					// [AI] Decrement index so the element shifted into this slot
					// is not skipped by the loop's ++index. Matches the established
					// pattern in VK_StagingBufferManager::ProcessPendingFree.
					index--;
				}
			}
		}
	}

	for (Int index = 0; index < buffer_allocations_to_release.size(); ++index)
	{
		VK_MemoryResourceFragmentAllocator* buffer_allocation = buffer_allocations_to_release[index];

		buffer_allocation->Destroy(device);

		VK_MemoryResourceHeap* heap = resource_heaps[buffer_allocation->memory_type_index];
		heap->ReleasePage(buffer_allocation);
	}

	device_memory_manager->TrimMemory(is_immediately);
}

VK_MemoryManager::~VK_MemoryManager()
{
	Destroy();
}

Bool VK_MemoryManager::AllocateUniformBuffer(VK_Allocation& out_allocation, UInt32 in_size)
{
	return (AllocateBufferPooled(out_allocation, in_size, 0, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ENUM_VK_AllocationMetaType::EVulkanAllocationMetaUniformBuffer));
}

void VK_MemoryManager::FreeUniformBuffer(VK_Allocation& in_allocation)
{
	FreeAllocation(in_allocation);
}

VK_MemoryResourceFragmentAllocator::~VK_MemoryResourceFragmentAllocator()
{
	if (!MergeFreeBlocks())
	{
		UInt32 leak_count = 0;
		for (VK_AllocationInternalInfo& data : internal_data)
		{
			CHECK_WITH_LOG_WARNING(data.state == VK_AllocationInternalInfo::ENUM_VK_AllocationState::EALLOCATED,
				"RHI WARNING :VK_MemoryResourceFragmentAllocator::~VK_MemoryResourceFragmentAllocator dont release ! ")
			leak_count++;
		}
	}
}

Int VK_MemoryResourceFragmentAllocator::AllocateInternalData()
{
	Int free_listnode_head = internal_free_listnode_index;
	if (free_listnode_head < 0)
	{
		internal_data.emplace_back();
        Int result = internal_data.size()-1;
        internal_data[result].next_free = -1;
		return result;

	}
	else
	{
        internal_free_listnode_index = internal_data[free_listnode_head].next_free;
		internal_data[free_listnode_head].next_free = -1;
		return free_listnode_head;
	}
}

void VK_MemoryResourceFragmentAllocator::FreeInternalData(Int index)
{
	CHECK_WITH_LOG(!(internal_data[index].state == VK_AllocationInternalInfo::ENUM_VK_AllocationState::EUNUSED || internal_data[index].state == VK_AllocationInternalInfo::ENUM_VK_AllocationState::EFREED),
        "RHI Error : failed to free internal data because internaldata.state error!")
    CHECK_WITH_LOG(internal_data[index].next_free != -1,
        "RHI Error : failed to free vk_allocation because internaldata is free!")
    internal_data[index].next_free = internal_free_listnode_index;
    internal_free_listnode_index = index;
    internal_data[index].state = VK_AllocationInternalInfo::ENUM_VK_AllocationState::EUNUSED;
}

void VK_MemoryResourceFragmentAllocator::Destroy(VK_Device* device)
{
	if (buffer)
	{
		vkDestroyBuffer(device->GetDevice(), buffer, VULKAN_CPU_ALLOCATOR);
		buffer = 0;
	}

}

Bool VK_MemoryResourceFragmentAllocator::MergeFreeBlocks()
{
    VK_Section::MergeConsecutiveRanges(free_list);
	if (free_list.size() == 1)
	{
		if (num_sub_allocations == 0)
		{
			//check(UsedSize == 0);
			//checkf(FreeList[0].Offset == 0 && FreeList[0].Size == MaxSize, TEXT("Resource Suballocation leak, should have %d free, only have %d; missing %d bytes"), MaxSize, FreeList[0].Size, MaxSize - FreeList[0].Size);
			return true;
		}
	}
	return false;
}

VK_MemoryResourceFragmentAllocator::VK_MemoryResourceFragmentAllocator(ENUM_VK_AllocationType in_type, VK_MemoryManager* in_owner, UInt8 in_subresource_allocator_flags, VK_DeviceMemoryAllocation* in_device_memory_allocation, UInt32 in_memory_type_index, UInt32 buffer_id /*= 0xffffffff*/)
:type(in_type),owner_memory_manager(in_owner),subresource_allocator_flags(in_subresource_allocator_flags),memory_allocation(in_device_memory_allocation),memory_type_index(in_memory_type_index),buffer_id(buffer_id)
{
	max_size = in_device_memory_allocation->GetSize();

	if (in_device_memory_allocation->CheckIsMapped())
	{
		subresource_allocator_flags |= VulkanAllocationFlagsMapped;
	}
	else
	{
		subresource_allocator_flags &= ~VulkanAllocationFlagsMapped;
	}

	VK_Section full_range;
	full_range.offset = 0;
	full_range.size = max_size;
	free_list.push_back(full_range);
}

VK_MemoryResourceFragmentAllocator::VK_MemoryResourceFragmentAllocator(ENUM_VK_AllocationType in_type, VK_MemoryManager* in_owner, UInt8 in_subresource_allocator_flags, VK_DeviceMemoryAllocation* in_device_memory_allocation, UInt32 in_memory_type_index, VkMemoryPropertyFlags in_memory_property_flags, UInt32 in_alignment, VkBuffer in_buffer, UInt32 in_buffer_size, UInt32 in_buffer_id, VkBufferUsageFlags in_buffer_usage_flags, Int in_pool_size_index)
	: type(in_type), owner_memory_manager(in_owner), subresource_allocator_flags(in_subresource_allocator_flags), memory_allocation(in_device_memory_allocation), memory_type_index(in_memory_type_index), buffer_id(in_buffer_id),
	max_size(in_buffer_size),buffer_usage_flags(in_buffer_usage_flags),buffer(in_buffer),pool_size_index(in_pool_size_index),allocator_index(0xffffffff),alignment(in_alignment)
{
	if (in_device_memory_allocation->CheckIsMapped())
	{
		subresource_allocator_flags |= VulkanAllocationFlagsMapped;
	}
	else
	{
		subresource_allocator_flags &= ~VulkanAllocationFlagsMapped;
	}

	VK_Section full_range;
	full_range.offset = 0;
	full_range.size = max_size;
	free_list.push_back(full_range);
}

void* VK_MemoryResourceFragmentAllocator::GetMapPointer()
{
	return memory_allocation->GetMappedPointer();
}

UInt32 VK_MemoryResourceFragmentAllocator::GetAlignment() CONST
{
	return alignment;
}

VK_DeviceMemoryAllocation* VK_MemoryResourceFragmentAllocator::GetMemoryAllocation()
{
	return memory_allocation;
}

void VK_MemoryResourceFragmentAllocator::FlushMappedMemory(UInt32 in_size, UInt32 offset)
{
	memory_allocation->FlushMappedMemory(in_size,offset);
}

void VK_MemoryResourceFragmentAllocator::InvalidateMappedMemory(UInt32 in_size, UInt32 offset)
{
	memory_allocation->InvalidateMappedMemory(in_size, offset);
}

void VK_AllocationInternalInfo::Init(CONST VK_Allocation& alloc, VK_Evictable* in_allocation_owner, UInt32 in_allocation_offset, UInt32 in_allocation_size, UInt32 in_alignment)
{
	CHECK_WITH_LOG(state != ENUM_VK_AllocationState::EUNUSED,"RHI Error: failed to create allocation internal info !")
    state =  ENUM_VK_AllocationState::EALLOCATED;
	type = alloc.GetType();
	meta_type = alloc.meta_type;

	size = alloc.size;
	allocation_size = in_allocation_size;
	allocation_offset = in_allocation_offset;
	allocation_owner = in_allocation_owner;
	alignment = in_alignment;
}

void VK_Allocation::Init(ENUM_VK_AllocationType in_type, ENUM_VK_AllocationMetaType in_meta_type, UInt64 in_handle, UInt32 in_size, UInt32 in_aligned_offset, UInt32 in_allocator_index, UInt32 in_allocation_index, UInt32 in_buffer_id)
{
	SetType(in_type);
	meta_type = in_meta_type;
	vulkan_handle = in_handle;
	size = in_size;
	offset = in_aligned_offset;
	allocator_index = in_allocator_index;
	allocation_index = in_allocation_index;
	handle_id = in_buffer_id;
	is_has_owner_ship = 1;
}

void VK_Allocation::Free(VK_Device& device)
{
	if(HasAllocation())
	{
		device.GetMemoryManager()->FreeAllocation(*this);
	}

}

void VK_Allocation::Swap(VK_Allocation& other)
{
	std::swap(*this, other);
}

void VK_Allocation::Reference(CONST VK_Allocation& other)
{
     memcpy(this, &other, sizeof(VK_Allocation));
	 is_has_owner_ship=0;
}

Bool VK_Allocation::HasAllocation()
{
	return type != (UInt8)ENUM_VK_AllocationType::EVulkanAllocationEmpty && is_has_owner_ship;
}

void VK_Allocation::SetOwnerShipFalse()
{
	is_has_owner_ship = 0;
}

void VK_Allocation::SetOwnerShipTrue()
{
	is_has_owner_ship = 1;
}

Bool VK_Allocation::IsValid() CONST
{
	return size != 0;
}

VkBuffer VK_Allocation::GetBufferHandle() CONST
{
	return (VkBuffer)vulkan_handle;
}

UInt32 VK_Allocation::GetBufferAlignment(VK_Device* device) CONST
{
	VK_MemoryResourceFragmentAllocator* allocator = GetSubresourceAllocator(device);
	return allocator->GetAlignment();
}

UInt32 VK_Allocation::GetBufferOffset() CONST
{
	return offset;
}

VkDeviceMemory VK_Allocation::GetDeviceMemoryHandle(VK_Device* device) CONST
{
	VK_MemoryResourceFragmentAllocator* allocator = GetSubresourceAllocator(device);
	return allocator->GetMemoryAllocation()->GetMemory();
}

void VK_Allocation::BindBuffer(VK_Device* device, VkBuffer buffer)
{
	VkDeviceMemory memory = GetDeviceMemoryHandle(device);
	VkResult result = vkBindBufferMemory(device->GetDevice(), buffer, memory, offset);
	CHECK_WITH_LOG(result != VK_SUCCESS, "RHI Error: failed to bind buffer memory !")
}

void VK_Allocation::BindImage(VK_Device* device, VkImage image)
{
	VkDeviceMemory memory = GetDeviceMemoryHandle(device);
	VkResult result = vkBindImageMemory(device->GetDevice(), image, memory, offset);
	CHECK_WITH_LOG(result != VK_SUCCESS, "RHI Error: failed to bind image memory !")
}

VK_Allocation::VK_Allocation()
{
	SetType( ENUM_VK_AllocationType::EVulkanAllocationEmpty);
}

void* VK_Allocation::GetMappedPointer(VK_Device* device)
{
	VK_MemoryResourceFragmentAllocator* allocator = GetSubresourceAllocator(device);
	return (UInt8*)(allocator->GetMapPointer()) + offset;
}

void VK_Allocation::InvalidateMappedMemory(VK_Device* device)
{
	VK_MemoryResourceFragmentAllocator* allocator = GetSubresourceAllocator(device);
	allocator->InvalidateMappedMemory(size,offset);
}

void VK_Allocation::FlushMappedMemory(VK_Device* device)
{
	VK_MemoryResourceFragmentAllocator* allocator = GetSubresourceAllocator(device);
	allocator->FlushMappedMemory(offset, size);
}

VK_MemoryResourceFragmentAllocator* VK_Allocation::GetSubresourceAllocator(VK_Device* device) CONST
{
	switch ((ENUM_VK_AllocationType)type)
	{
	case ENUM_VK_AllocationType::EVulkanAllocationEmpty:
		return nullptr;
		break;
	case ENUM_VK_AllocationType::EVulkanAllocationBuffer :
	case ENUM_VK_AllocationType::EVulkanAllocationImage:
	case ENUM_VK_AllocationType::EVulkanAllocationImageDedicated:
	case ENUM_VK_AllocationType::EVulkanAllocationPooledBuffer:
		return device->GetMemoryManager()->GetSubresourceAllocator(allocator_index);
		break;
	default:
		CHECK_WITH_LOG(true, "RHI Error: failed to get subresource allocator !")
		break;
	}
	return nullptr;
}

Int VK_Section::Add(Vector<VK_Section>& ranges, CONST VK_Section& item)
{
#if	VK_MEMORY_KEEP_FREELIST_SORTED
	Int num_ranges = ranges.size();
	if (num_ranges <= 0)
	{
		ranges.push_back(item);
		return ranges.size() - 1;
	}

	VK_Section* data = ranges.data();
	for (Int index = 0; index < num_ranges; ++index)
	{
		if ((data[index].offset > item.offset))
		{
			return InsertAndTryToMerge(ranges, item, index);
		}
	}
	return AppendAndTryToMerge(ranges, item);
#else
	return ranges.push_back(item);
#endif
}

Int VK_Section::InsertAndTryToMerge(Vector<VK_Section>& ranges, CONST VK_Section& item, Int proposed_index)
{
#if !VK_MEMORY_JOIN_FREELIST_ON_THE_FLY
	Int ret = proposed_index;
	auto it = ranges.begin() + proposed_index;
	ranges.insert(it, item);
#else
	// there are four cases here
	// 1) nothing can be merged (distinct ranges)		 XXXX YYY ZZZZZ  =>   XXXX YYY ZZZZZ
	// 2) new range can be merged with the previous one: XXXXYYY  ZZZZZ  =>   XXXXXXX  ZZZZZ
	// 3) new range can be merged with the next one:     XXXX  YYYZZZZZ  =>   XXXX  ZZZZZZZZ
	// 4) new range perfectly fills the gap:             XXXXYYYYYZZZZZ  =>   XXXXXXXXXXXXXX

	// note: we can have a case where we're inserting at the beginning of the array (no previous element), but we won't have a case
	// where we're inserting at the end (no next element) - AppendAndTryToMerge() should be called instead
	CHECK_WITH_LOG(!(item.offset < ranges[proposed_index].offset), 
		("RHI Error: VK_Section::InsertAndTryToMerge() was called to append an element - internal logic error, VK_Section::AppendAndTryToMerge() should have been called instead."))
	Int ret = proposed_index;
	if (proposed_index == 0)
	{
		VK_Section& next_range = ranges[ret];

		if (next_range.offset == item.offset + item.size)
		{
			next_range.offset = item.offset;
			next_range.size += item.size;
		}
		else
		{
			auto ret_it = ranges.insert(ranges.begin(),item);
			ret = 0;
		}
	}
	else
	{
		// all cases apply
		VK_Section& next_range = ranges[proposed_index];
		VK_Section& prev_range = ranges[proposed_index - 1];

		// see if we can merge with previous
		if (prev_range.offset + prev_range.size	 == item.offset)
		{
			// case 2, can still end up being case 4
			prev_range.size += item.size;

			if (prev_range.offset + prev_range.size == next_range.offset)
			{
				// case 4
				prev_range.size += next_range.size;
				auto find_res= std::find(ranges.begin(), ranges.end(), next_range);
				if(find_res!=ranges.end())
					ranges.erase(find_res);
				ret = proposed_index - 1;
			}
		}
		else if (item.offset + item.size == next_range.offset)
		{
			// case 3
			next_range.offset = item.offset;
			next_range.size += item.size;
		}
		else
		{
			// case 1 - the new range is disjoint with both
			ret = proposed_index;
			auto it = ranges.begin() + proposed_index;
			ranges.insert(it, item);	// this can invalidate NextRange/PrevRange references, don't touch them after this
		}
	}
#endif
#if VK_MEMORY_KEEP_FREELIST_SORTED_CATCHBUGS
	SanityCheck(ranges);
#endif
	return ret;
}

Int VK_Section::AppendAndTryToMerge(Vector<VK_Section>& ranges, CONST VK_Section& item)
{
#if !VK_MEMORY_JOIN_FREELIST_ON_THE_FLY
	ranges.push_back(item);
	Int ret = ranges.size() - 1;
#else
	Int ret = ranges.size() - 1;
	// we only get here when we have an element in front of us
	CHECK_WITH_LOG((ret < 0), 
		("RHI Error: VK_Section::AppendAndTryToMerge() was called on an empty array."))
	VK_Section& prev_range = ranges[ret];
	if (prev_range.offset + prev_range.size == item.offset)
	{
		prev_range.size += item.size;
	}
	else
	{
		ranges.push_back(item);
		ret = ranges.size() - 1;
	}
#endif
#if VK_MEMORY_KEEP_FREELIST_SORTED_CATCHBUGS
	SanityCheck(ranges);
#endif
	return ret;
}

void VK_Section::AllocateFromEntry(Vector<VK_Section>& ranges, Int index, UInt32 size_to_allocate)
{
	VK_Section& entry = ranges[index];
	if (size_to_allocate < entry.size)
	{
		// Modify current free entry in-place.
		entry.size -= size_to_allocate;
		entry.offset += size_to_allocate;
	}
	else
	{
		// Remove this free entry.
		auto it = ranges.begin() + index;
		ranges.erase(it);
#if VK_MEMORY_KEEP_FREELIST_SORTED_CATCHBUGS
		SanityCheck(ranges);
#endif
	}
}

void VK_Section::SanityCheck(Vector<VK_Section>& ranges)
{
#if VK_MEMORY_KEEP_FREELIST_SORTED_CATCHBUGS
	Int num_ranges = ranges.size();
	if (num_ranges <= 0)
	{
		return;

	}

	VK_Section* data = ranges.data();
	for (Int index = 0; index < num_ranges - 1; ++index)
	{
		CHECK_WITH_LOG((data[index].offset + data[index].size >= data[index + 1].offset), 
					("RHI Error: VK_Section::SanityCheck() failed :Ranges are overlapping or adjoining!"))
		CHECK_WITH_LOG((data[index].offset >= data[index + 1].offset),
			("RHI Error: VK_Section::SanityCheck() failed : Array is not sorted!"))
	}
#endif
}

void VK_Section::MergeConsecutiveRanges(Vector<VK_Section>& ranges)
{
	if (ranges.size() > 1)
	{
		SanityCheck(ranges);
		for(Int index = ranges.size() - 1; index > 0; --index)
		{
			VK_Section& current = ranges[index];
			VK_Section& pre = ranges[index - 1];
			if (pre.offset + pre.size == current.offset)
			{
				pre.size += current.size;
				auto it = ranges.begin() + index;
				ranges.erase(it);
			}
		}
	}
}

size_t MemoryBlockKey::operator()(CONST MemoryBlockKey& p) CONST
{
	return HashCombine(std::hash<UInt32>()(p.block_size), std::hash<UInt32>()(p.memory_type_index));
}

Bool MemoryBlockKey::operator==(CONST MemoryBlockKey& other) CONST
{
	return memory_type_index == other.memory_type_index && block_size == other.block_size;
}

Bool MemoryBlockKey::operator<(CONST MemoryBlockKey& other) CONST
{
	if (memory_type_index < other.memory_type_index)
	{
		return true;
	}
	else if (memory_type_index == other.memory_type_index)
	{
		return block_size < other.block_size;
	}
	else
	{
		return false;
	}
}

MemoryBlockKey& MemoryBlockKey::operator=(CONST MemoryBlockKey& other)
{
	memory_type_index = other.memory_type_index;
	block_size = other.block_size;
	return *this;
}

MemoryBlockKey::MemoryBlockKey(UInt32 in_memory_type_index, VkDeviceSize in_block_size)
{
	memory_type_index = in_memory_type_index;
	block_size = in_block_size;
}

MemoryBlockKey::MemoryBlockKey(CONST MemoryBlockKey& other) 
{
	memory_type_index = other.memory_type_index;
	block_size = other.block_size;
}

VkMemoryPropertyFlags VK_MemoryManager::GetMemoryPropertyFlags(ENUM_VulkanAllocationFlags in_alloc_flags, Bool is_has_unified_memory)
{
	return InternalGetMemoryPropertyFlags(in_alloc_flags, is_has_unified_memory);
}

MYRENDERER_END_NAMESPACE

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

