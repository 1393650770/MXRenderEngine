#pragma once
#ifndef _VK_MEMORY_
#define _VK_MEMORY_

#include <vulkan/vulkan_core.h>
#include "Core/ConstDefine.h"
#include "RHI/RenderResource.h"

#define VULKAN_MEMORY_LOW_PRIORITY 0.f
#define VULKAN_MEMORY_MEDIUM_PRIORITY 0.5f
#define VULKAN_MEMORY_HIGHER_PRIORITY 0.75f
#define VULKAN_MEMORY_HIGHEST_PRIORITY 1.f

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

class VK_Device;
class VK_DeviceMemoryAllocation;
class VK_CommandBuffer;
class VK_DeviceMemoryManager;
class VK_MemoryManager;
class VK_MemoryResourceHeap;
class VK_MemoryResourceFragmentAllocator;
class VK_Allocation;

enum class ENUM_VK_AllocationType : UInt8
{
	EVulkanAllocationEmpty=0,
	EVulkanAllocationPooledBuffer,
	EVulkanAllocationBuffer,
	EVulkanAllocationImage,
	EVulkanAllocationImageDedicated,
	EVulkanAllocationTempBlockBuffer,
};

enum class ENUM_VK_AllocationMetaType : UInt8
{
	EVulkanAllocationMetaUnknown=0,
	EVulkanAllocationMetaUniformBuffer,
	EVulkanAllocationMetaMultiBuffer,
	EVulkanAllocationMetaRingBuffer,
	EVulkanAllocationMetaFrameTempBuffer,
	EVulkanAllocationMetaImageRenderTarget,
	EVulkanAllocationMetaImageOther,
	EVulkanAllocationMetaBufferUAV,
	EVulkanAllocationMetaBufferStaging,
	EVulkanAllocationMetaBufferOther,
	EVulkanAllocationMetaSize,
};

enum
{
	GPU_ONLY_HEAP_PAGE_SIZE = 128 * 1024 * 1024,
	STAGING_HEAP_PAGE_SIZE = 32 * 1024 * 1024,
	ANDROID_MAX_HEAP_PAGE_SIZE = 16 * 1024 * 1024,
	ANDROID_MAX_HEAP_IMAGE_PAGE_SIZE = 16 * 1024 * 1024,
	ANDROID_MAX_HEAP_BUFFER_PAGE_SIZE = 4 * 1024 * 1024,
};

enum class ENUM_VK_HeapAllocationType :UInt32
{
    None=0,
    Image,
    Buffer,
    Count
};

enum ENUM_VK_LegacyVulkanAllocationFlags
{
	VulkanAllocationFlagsMapped = 0x1,
	VulkanAllocationFlagsCanEvict = 0x2,
};

enum class ENUM_VulkanAllocationFlags : UInt16
{
	None = 0x0000,

	HostVisible = 0x0001,    // Will be written from CPU, will likely contain the HOST_VISIBLE + HOST_COHERENT flags
	HostCached = 0x0002,    // Will be used for readback, will likely contain the HOST_CACHED flag.  Implies HostVisible.
	PreferBAR = 0x0004,    // Will be allocated from a HOST_VISIBLE + DEVICE_LOCAL heap if available and possible (HOST_VISIBLE if not)

	Dedicated = 0x0008,	  // Will not share a memory block with other resources
	External = 0x0010,    // To be used with VK_KHR_external_memory
	Memoryless = 0x0020,	  // Will use LAZILY_ALLOCATED

	NoError = 0x0040,    // OOM is not fatal, return an invalid allocation
	AutoBind = 0x0080,	  // Will automatically bind the allocation to the supplied VkBuffer/VkImage, avoids an extra lock to bind separately
};
ENUM_CLASS_FLAGS(ENUM_VulkanAllocationFlags)

enum class ENUM_DelayAcquireImageType : UInt32
{
	None,			// acquire next image on frame start
	DelayAcquire,	// acquire next image just before presenting, rendering is done to intermediate image which is copied to real backbuffer
	LazyAcquire,	// acquire next image on first use
};

extern ENUM_DelayAcquireImageType g_vulkan_delay_acquire_image;

extern Int g_vulkan_use_buffer_binning;

MYRENDERER_BEGIN_STRUCT(MemoryHeapInfo)
    VkDeviceSize used_size=0;
    VkDeviceSize max_size=0;
    Vector<VK_DeviceMemoryAllocation*> allocations;
MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_STRUCT(MemoryBlockKey)
    UInt32 memory_type_index=0;
    VkDeviceSize block_size=0;
    Bool operator==(CONST MemoryBlockKey& other) CONST;
    size_t operator()(CONST MemoryBlockKey& p) CONST;

    Bool operator<(CONST MemoryBlockKey& other) CONST;

    MemoryBlockKey(CONST MemoryBlockKey& other);
    MemoryBlockKey(UInt32 in_memory_type_index, VkDeviceSize in_block_size);
    MemoryBlockKey() MYDEFAULT;
    MemoryBlockKey& operator=(CONST MemoryBlockKey& other);
MYRENDERER_END_STRUCT


MYRENDERER_BEGIN_STRUCT(MemoryBlock)
    MemoryBlockKey key;

    MYRENDERER_BEGIN_STRUCT(FreeBlock)
        VK_DeviceMemoryAllocation* allocation;
        UInt32 frame_freed;
    MYRENDERER_END_STRUCT

    Vector<FreeBlock> allocations;

MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_CLASS(VK_Evictable)
#pragma region METHOD
public:
	virtual Bool CanMove() CONST { return false; }
	virtual void Move(VK_Device& device, VK_CommandBuffer* cmd, VK_Allocation& new_allocation) {}
	virtual VkBuffer GetVkBuffer() CONST { return VK_NULL_HANDLE; }
	virtual UInt32 GetMemorySize() CONST { return 0; }
	virtual UInt32 GetMemoryAlignment() CONST { return 256; }
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VK_DeviceMemoryAllocation,public RenderResource)

friend  VK_DeviceMemoryManager;

#pragma region METHOD
public:
    VK_DeviceMemoryAllocation();
    VIRTUAL ~VK_DeviceMemoryAllocation() MYDEFAULT;
    void* METHOD(Map)(VkDeviceSize in_size,VkDeviceSize offset);
    void METHOD(UnMap)();
    VkDeviceMemory METHOD(GetMemory)();
    VkDeviceSize METHOD(GetSize)();
    void* METHOD(GetMappedPointer)();
    void METHOD(FlushMappedMemory)(VkDeviceSize in_size,VkDeviceSize offset);
    void METHOD(InvalidateMappedMemory)(VkDeviceSize in_size,VkDeviceSize offset);
    Bool METHOD(GetIsCanBeMapped)() CONST;
    Bool METHOD(GetIsCoherent)()CONST;
    Bool METHOD(CheckIsMapped)() CONST;
protected:
    
private:

#pragma endregion

#pragma region MEMBER
public:
    union MemoryProperty
    {
        struct
        {
            UInt16 memory_type_index : 8;
            UInt16 is_can_be_mapped : 1;
            UInt16 is_coherent : 1;
            UInt16 is_cached : 1;
            UInt16 is_freed_by_system : 1;
            UInt16 is_dedicated_memory : 1;
            
        };
        UInt16 packed;
    };
protected:
    VkDeviceSize size= 0;
    VkDevice device_handle=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    void* mapped_pointer=nullptr;

    MemoryProperty property;

private:

#pragma endregion
MYRENDERER_END_CLASS


MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VK_DeviceMemoryManager,public RenderResource)

friend class VK_MemoryManager;

#pragma region METHOD
public: 
    VK_DeviceMemoryManager(VK_Device * in_device);
    VIRTUAL ~VK_DeviceMemoryManager();
    void METHOD(Destroy)();
    VK_DeviceMemoryAllocation* METHOD(Alloc)(VkDeviceSize allocation_size, UInt32 memory_type_index, void* dedicated_allocate_info, float priority, Bool is_external);

    void METHOD(Free)(VK_DeviceMemoryAllocation*& allocation);
    CONST UInt32 METHOD(GetMemoryTypeNum)() CONST;
    UInt32 METHOD(GetHeapIndex)(UInt32 memory_type_index);
    CONST VkPhysicalDeviceMemoryProperties& METHOD(GetMemoryProperties)() CONST;
    VkResult METHOD(GetMemoryTypeFromProperties)(UInt32 type_bits, VkMemoryPropertyFlags properties, UInt32* out_type_index);
    VkResult METHOD(GetMemoryTypeFromPropertiesExcluding)(UInt32 type_bits, VkMemoryPropertyFlags properties, UInt32 exclude_type_index, UInt32* out_type_index);
    Bool METHOD(GetIsSupportUnifiedMemory)() CONST;

	FORCEINLINE Bool METHOD(HasUnifiedMemory)() CONST
	{
		return is_support_unified_memory;
	}
    FORCEINLINE Bool METHOD(SupportsMemoryless)() CONST
	{
		return is_support_memory_less;
	}

    FORCEINLINE UInt32 METHOD(GetNumMemoryTypes)() CONST
	{
		return memory_properties.memoryTypeCount;
	}

protected:
    /// <summary>
    /// ������ͷ��ڴ�
    /// </summary>
    Int METHOD(FreeInternal)(VK_DeviceMemoryAllocation*& allocation);

    /// <summary>
    /// ��֡����ʱ����ڴ�(��ͳ����ɢ�Ŀ������ٰ�֡������һ��������ȫ�����)
    /// </summary>
    /// <param name=""></param>
    /// <returns></returns>
    void METHOD(TrimMemory)(Bool is_full_trim);
private:

#pragma endregion

#pragma region MEMBER
public:

protected:
    VK_Device* device=nullptr;
    UInt32 num_allocations=0;
    UInt32 max_num_allocations=0;
    VkPhysicalDeviceMemoryBudgetPropertiesEXT memory_budget;
    VkPhysicalDeviceMemoryProperties memory_properties;
    Int primary_heap_index=0;
    Bool is_support_lazily_allocated=false;
    Vector<MemoryHeapInfo> memory_heaps;
    Map<MemoryBlockKey,MemoryBlock, MemoryBlockKey> memory_block_map;
	Bool is_support_unified_memory=false;
    Bool is_support_memory_less=false;

private:

#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_BEGIN_STRUCT(VK_VulkanPageSizeBucket)

    UInt64 max_allocation = 0;
    UInt32 page_size = 0;
    enum
    {
	    BUCKET_MASK_IMAGE = 0x1,
	    BUCKET_MASK_BUFFER = 0x2,
    };
    UInt32 bucket_mask = BUCKET_MASK_BUFFER;

MYRENDERER_END_STRUCT

/// <summary>
/// �������Ƭ����
/// </summary>
MYRENDERER_BEGIN_STRUCT(VK_Section)
    UInt32 offset;
    UInt32 size;

    Bool operator<(CONST VK_Section& in) CONST
    {
	    return offset < in.offset;
    }

    Bool operator==(CONST VK_Section& in) CONST
	{
	    return offset == in.offset && size == in.size;
	}

    static void METHOD(MergeConsecutiveRanges)(Vector<VK_Section>& ranges);

    /** Tries to insert the item so it has index ProposedIndex, but may end up merging it with neighbors */
    static Int METHOD(InsertAndTryToMerge)(Vector<VK_Section>& ranges, CONST VK_Section& item, Int proposed_index);

    /** Tries to append the item to the end but may end up merging it with the neighbor */
    static Int METHOD(AppendAndTryToMerge)(Vector<VK_Section>& ranges, CONST VK_Section& item);

    /** Attempts to allocate from an entry - can remove it if it was used up*/
    static void METHOD(AllocateFromEntry)(Vector<VK_Section>& ranges, Int index, UInt32 size_to_allocate);

    /** Sanity checks an array of ranges */
    static void METHOD(SanityCheck)(Vector<VK_Section>& ranges);

    /** Adds to the array while maintaing the sort. */
    static Int METHOD(Add)(Vector<VK_Section>& ranges, CONST VK_Section& item);

MYRENDERER_END_STRUCT


/// <summary>
/// ������¼��Ϣ��������Ǩ�ƺ���Ƭ����
/// </summary>
MYRENDERER_BEGIN_STRUCT(VK_AllocationInternalInfo)
    enum class ENUM_VK_AllocationState :Int
    {
	    EUNUSED =0,
	    EALLOCATED,
	    EFREED,
	    EFREEPENDING,
	    EFREEDISCARDED, // if a defrag happens with a pending free, its put into this state, so we can ignore the deferred delete once it happens.
    };


    void METHOD(Init)(CONST VK_Allocation& alloc, VK_Evictable* in_allocation_owner, UInt32 allocation_offset, UInt32 allocation_size, UInt32 alignment);

    ENUM_VK_AllocationState state = ENUM_VK_AllocationState::EUNUSED;
    ENUM_VK_AllocationType type = ENUM_VK_AllocationType::EVulkanAllocationEmpty;
    ENUM_VK_AllocationMetaType meta_type = ENUM_VK_AllocationMetaType::EVulkanAllocationMetaUnknown;

    UInt32 size = 0;
    UInt32 allocation_size = 0;
    UInt32 allocation_offset = 0;
    VK_Evictable* allocation_owner = 0;
    UInt32 alignment = 0;

    Int next_free = -1;

MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_CLASS(VK_MemoryResourceFragmentAllocator)
friend class VK_MemoryManager;
friend class VK_MemoryResourceHeap;

#pragma region METHOD
public:
	VK_MemoryResourceFragmentAllocator(ENUM_VK_AllocationType in_type, VK_MemoryManager* in_owner, UInt8 in_subresource_allocator_flags, VK_DeviceMemoryAllocation* in_device_memory_allocation, UInt32 in_memory_type_index, UInt32 buffer_id = 0xffffffff);
    VK_MemoryResourceFragmentAllocator(ENUM_VK_AllocationType in_type, VK_MemoryManager* in_owner, UInt8 in_subresource_allocator_flags, VK_DeviceMemoryAllocation* in_device_memory_allocation, UInt32 in_memory_type_index, 
        VkMemoryPropertyFlags in_memory_property_flags,UInt32 in_alignment, VkBuffer in_buffer, UInt32 in_buffer_size, UInt32 in_buffer_id, VkBufferUsageFlags in_buffer_usage_flags, Int in_pool_size_index);

    VIRTUAL ~VK_MemoryResourceFragmentAllocator();

    Bool METHOD(TryAllocate)(VK_Allocation& out_allocation, VK_Evictable* owner, UInt32 in_size, UInt32 in_alignment, ENUM_VK_AllocationMetaType in_meta_type);
    void METHOD(SetAllocationOwner)(UInt32 alloc_index, VK_Evictable* owner);
    void METHOD(Free)(VK_Allocation& allocation);
    void METHOD(Destroy)(VK_Device* device);
    UInt8 METHOD(GetSubresourceAllcatorFlags)();
    UInt32 METHOD(GetAlignment)() CONST;
    void* METHOD(GetMapPointer)();
    void METHOD(FlushMappedMemory)(UInt32 in_size, UInt32 offset);
    void METHOD(InvalidateMappedMemory)(UInt32 in_size, UInt32 offset);
    VK_DeviceMemoryAllocation* METHOD(GetMemoryAllocation)() ;
protected:
    Int METHOD(AllocateInternalData)();
    void METHOD(FreeInternalData)(Int index);
    Bool METHOD(MergeFreeBlocks)();
	Bool METHOD(CanDefrag)() CONST;
	UInt32 METHOD(DefragTick)(VK_Device& device, VK_CommandBuffer* cmd, VK_MemoryResourceHeap* heap, UInt32 max_count);
private:

#pragma endregion

#pragma region MEMBER
public:
protected:
    UInt32 memory_used[(UInt32)ENUM_VK_AllocationMetaType::EVulkanAllocationMetaSize];
    ENUM_VK_AllocationType type;
    VK_MemoryManager* owner_memory_manager = nullptr;
    UInt32 memory_type_index=0;
    VkMemoryPropertyFlags memory_property_flags;
	VK_DeviceMemoryAllocation* memory_allocation;
    UInt32 max_size=0;
    UInt32 alignment=0;
    UInt32 frame_freed=0;
    UInt32 last_defrag_frame=0;
    Int64 used_size=0;
	VkBufferUsageFlags buffer_usage_flags;
	VkBuffer buffer;
    UInt32 buffer_id;
    Int pool_size_index;
    UInt32 allocator_index;
    UInt8 subresource_allocator_flags;
    UInt8 bucket_id;
	Bool is_evicting = false;
    Bool is_locked = false;
    Bool is_defragging = false;

    UInt32 num_sub_allocations = 0;
    UInt32 alloc_calls = 0;
    UInt32 free_calls = 0;
    Vector<VK_Section> free_list;
    Vector<VK_AllocationInternalInfo> internal_data;
	Vector<VK_Allocation*> allocation_refs; // parallel to internal_data, for defrag back-reference
    Int internal_free_listnode_index=-1;
private:

#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_BEGIN_CLASS(VK_MemoryResourceHeap)
friend class VK_MemoryManager;
friend class VK_MemoryResourceFragmentAllocator;

#pragma region METHOD
public:
    VK_MemoryResourceHeap(VK_MemoryManager* in_owner, UInt32 in_memory_type_index, UInt32 in_override_page_size = 0);
    VIRTUAL ~VK_MemoryResourceHeap();

	void METHOD(FreePage)(VK_MemoryResourceFragmentAllocator* InPage);
	void METHOD(ReleasePage)(VK_MemoryResourceFragmentAllocator* InPage);

    UInt32 METHOD(GetPageSizeBucket)(VK_VulkanPageSizeBucket& out_bucket,ENUM_VK_HeapAllocationType type, UInt32 allocation_size,Bool is_force_single_allocation);
protected:
    Bool METHOD(TryRealloc)(VK_Allocation& out_allocation, VK_Evictable* allocation_owner, ENUM_VK_HeapAllocationType type, UInt32 size, UInt32 alignment, ENUM_VK_AllocationMetaType meta_type);
	void METHOD(DefragTick)(VK_Device& device, UInt32 count);
	void METHOD(SetDefragging)(VK_MemoryResourceFragmentAllocator* page);
    Bool METHOD(AllocateResource)(VK_Allocation& out_allocation,  VK_Evictable* allocation_owner, ENUM_VK_HeapAllocationType type, UInt32 size, UInt32 alignment, Bool is_map_allocation, Bool is_force_separate_allocation, ENUM_VK_AllocationMetaType meta_type, Bool is_external);
    Bool METHOD(AllocateDedicatedImage)(VK_Allocation& out_allocation,  VK_Evictable* allocation_owner, ENUM_VK_HeapAllocationType type, UInt32 size, UInt32 alignment, Bool is_map_allocation, Bool is_force_separate_allocation, ENUM_VK_AllocationMetaType meta_type, Bool is_external);
private:

#pragma endregion 
#pragma region MEMBER
public:

protected:
	enum {
		MAX_BUCKETS = 5,
	};
    Vector<VK_VulkanPageSizeBucket> page_size_buckets;
    VK_MemoryManager* owner;
    UInt32 memory_type_index=0;
    UInt32 override_page_size=0;
    UInt16 heap_index=0;
    Bool is_support_host_cached=false;
    Bool is_support_lazily_allocated=false;
    UInt8 defrag_count_down=0;
    UInt32 peak_page_size = 0;
    UInt64 used_memory=0;
    UInt64 page_id_counter=0;

    Vector<VK_MemoryResourceFragmentAllocator*> active_pages[MAX_BUCKETS];
    Vector<VK_MemoryResourceFragmentAllocator*> used_dedicated_pages;
private:

#pragma endregion
MYRENDERER_END_CLASS



MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VK_Allocation,public RenderResource)
#pragma region METHOD
public:
    VK_Allocation();
    VIRTUAL ~VK_Allocation() MYDEFAULT;
    void METHOD(Init)(ENUM_VK_AllocationType in_type, ENUM_VK_AllocationMetaType in_meta_type, UInt64 in_handle, UInt32 in_size, UInt32 in_aligned_offset, UInt32 in_allocator_index, UInt32 in_allocation_index, UInt32 in_buffer_id);
    void METHOD(Free)(VK_Device& device);
    void METHOD(Swap)(VK_Allocation& other);
    void METHOD(Reference)(CONST VK_Allocation& other); //point to other, but don't take ownership
    Bool METHOD(HasAllocation)();
    void METHOD(SetOwnerShipTrue)(); //disown & own should be used if ownership is transferred.
    void METHOD(SetOwnerShipFalse)(); //disown & own should be used if ownership is transferred.
    Bool METHOD(IsValid)() CONST;

    inline ENUM_VK_AllocationType METHOD(GetType)() CONST
    {
	    return (ENUM_VK_AllocationType)type;
    }
    inline void METHOD(SetType)(ENUM_VK_AllocationType in_type)
    {
	    type = (UInt8)in_type;
    }
    
    void* METHOD(GetMappedPointer)(VK_Device* device);
    void METHOD(FlushMappedMemory)(VK_Device* device);
    void METHOD(InvalidateMappedMemory)(VK_Device* device);
    VkBuffer METHOD(GetBufferHandle)() CONST;
    UInt32 METHOD(GetBufferAlignment)(VK_Device* device) CONST;
    UInt32 METHOD(GetBufferOffset)() CONST;
    VkDeviceMemory METHOD(GetDeviceMemoryHandle)(VK_Device* device) CONST;
    VK_MemoryResourceFragmentAllocator* METHOD(GetSubresourceAllocator)(VK_Device* device) CONST;
    void METHOD(BindBuffer)(VK_Device* device, VkBuffer buffer);
    void METHOD(BindImage)(VK_Device* device, VkImage image);

protected:

private:

#pragma endregion 
#pragma region MEMBER
public:
	UInt64 vulkan_handle = 0;
    UInt32 handle_id = 0;
    UInt32 size = 0;
    UInt32 offset = 0;
    UInt32 allocation_index = 0;
    UInt16 allocator_index = 0;
	ENUM_VK_AllocationMetaType meta_type = ENUM_VK_AllocationMetaType::EVulkanAllocationMetaUnknown;
    //type= ENUM_VK_AllocationType
    UInt8 type : 7;
    UInt8 is_has_owner_ship : 1;

protected:

private:

#pragma endregion 

MYRENDERER_END_CLASS


MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VK_MemoryManager,public RenderResource)

friend class VK_MemoryResourceHeap;
friend class VK_Allocation;


#pragma region METHOD
public:
    VK_MemoryManager(VK_Device* in_device);
    VIRTUAL ~VK_MemoryManager();

    void METHOD(Destroy)();
    void METHOD(ReleaseFreedPages)();
	void METHOD(DefragTick)();
    Bool METHOD(AllocateBufferPooled)(VK_Allocation& out_allocation, UInt32 in_size, UInt32 in_min_alignment, VkBufferUsageFlags in_buffer_usage_flags, VkMemoryPropertyFlags in_memory_property_flags, ENUM_VK_AllocationMetaType in_meta_type);
    Bool METHOD(AllocateImageMemory)(VK_Allocation& out_allocation, VkImage in_image, ENUM_VulkanAllocationFlags in_alloc_flags, UInt32 in_force_min_alignment = 1);
    Bool METHOD(AllocateBufferMemory)(VK_Allocation& out_allocation, VkBuffer in_buffer, ENUM_VulkanAllocationFlags in_alloc_flags, UInt32 in_force_min_alignment = 1);
    static VkMemoryPropertyFlags METHOD(GetMemoryPropertyFlags)(ENUM_VulkanAllocationFlags in_alloc_flags, Bool is_has_unified_memory);
    void METHOD(FreeAllocation)(VK_Allocation& allocation);
    //Bool METHOD(AllocateDedicatedImageMemory)(VK_Allocation* out_allocation);
    Bool METHOD(AllocateUniformBuffer)(VK_Allocation& out_allocation,UInt32 in_size);
    void METHOD(FreeUniformBuffer)(VK_Allocation& in_allocation);


	void METHOD(RegisterSubresourceAllocator)(VK_MemoryResourceFragmentAllocator* subresource_allocator);
	void METHOD(UnregisterSubresourceAllocator)(VK_MemoryResourceFragmentAllocator* subresource_allocator);
	void METHOD(ReleaseSubresourceAllocator)(VK_MemoryResourceFragmentAllocator* subresource_allocator);
    VK_DeviceMemoryManager* METHOD(GetDeviceMemoryManager)();
protected:
	VK_MemoryResourceFragmentAllocator* METHOD(GetSubresourceAllocator)(CONST UInt32 allocator_index);
    void METHOD(DestroyResourceAllocations)();
    void METHOD(ReleaseFreedResources)(Bool is_immediately);
private:
#pragma endregion 

#pragma region MEMBER
public:

protected:
	VK_DeviceMemoryManager* device_memory_manager;

	enum
	{
		BufferAllocationSize = 1 * 1024 * 1024,
		UniformBufferAllocationSize = 2 * 1024 * 1024,
	};


	// pool sizes that we support
	enum class EPoolSizes : UInt8
	{
		// 			E32,
		// 			E64,
		E128 = 0,
		E256,
		E512,
		E1k,
		E2k,
		E8k,
		E16k,
		SizesCount,
	};

	constexpr static UInt32 PoolSizes[(Int)EPoolSizes::SizesCount] =
	{
		// 			32,
		// 			64,
					128,
					256,
					512,
					1024,
					2048,
					8192,
					// 			16 * 1024,
	};

	constexpr static UInt32 BufferSizes[(Int)EPoolSizes::SizesCount + 1] =
	{
		// 			64 * 1024,
		// 			64 * 1024,
					128 * 1024,
					128 * 1024,
					256 * 1024,
					256 * 1024,
					512 * 1024,
					512 * 1024,
					1024 * 1024,
					1 * 1024 * 1024,
	};

	EPoolSizes GetPoolTypeForAlloc(UInt32 size, UInt32 alignment)
	{
		EPoolSizes pool_size = EPoolSizes::SizesCount;
		if (g_vulkan_use_buffer_binning != 0)
		{
			for (Int i = 0; i < (Int)EPoolSizes::SizesCount; ++i)
			{
				if (PoolSizes[i] >= size)
				{
					pool_size = (EPoolSizes)i;
					break;
				}
			}
		}
		return pool_size;
	}
	Vector<VK_MemoryResourceFragmentAllocator*> used_buffer_allocations[(Int)EPoolSizes::SizesCount + 1];
    Vector<VK_MemoryResourceFragmentAllocator*> free_buffer_allocations[(Int)EPoolSizes::SizesCount + 1];
    Vector<VK_MemoryResourceFragmentAllocator*> all_buffer_allocations;
    // [AI] Free-list of recycled allocator indices (replaces single-slot all_buffer_allocations_index).
    // Each index is reused at most once; eliminates page-index aliasing when multiple pages
    // are unregistered between registrations.
    Vector<Int> free_allocator_indices;
	UInt64 pending_evict_bytes = 0;
	Bool is_evicting = false;
    Bool is_want_eviction = false;


    VK_Device* device;
    Vector<VK_MemoryResourceHeap*> resource_heaps;
private:
#pragma endregion 

MYRENDERER_END_CLASS
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

namespace std
{
	template<>
	struct hash<MXRender::RHI::Vulkan::MemoryBlockKey>
	{
		size_t operator()(CONST MXRender::RHI::Vulkan::MemoryBlockKey& p) CONST
		{
			return p.operator()(p);
		}
	};
   
}

#endif
