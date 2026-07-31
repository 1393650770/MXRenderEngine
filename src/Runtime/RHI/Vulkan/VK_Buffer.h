#pragma once
#ifndef _VK_BUFFER_
#define _VK_BUFFER_
#include "RHI/RenderBuffer.h"
#include "VK_Device.h"


MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

class VK_CommandBuffer;

class VK_Buffer : public Buffer, public VK_Evictable
{
friend class VK_BufferManager;

#pragma region METHOD
public:
	VK_Buffer(VK_Device* in_device, CONST BufferDesc& in_buffer_desc);
	VIRTUAL ~VK_Buffer();

	VIRTUAL void* METHOD(Map)(CONST ENUM_MAP_TYPE& map_type, CONST ENUM_MAP_FLAG& map_flag) FINAL;
	VIRTUAL void METHOD(Unmap)()  FINAL;
	VIRTUAL void* METHOD(MapReadback)(UInt32 offset, UInt32 size, Float32 timeout_seconds = 1.0f) FINAL;
	VIRTUAL void METHOD(FreeReadback)(void* data) FINAL;
	VkBuffer METHOD(GetBuffer)() CONST;
	UInt32 METHOD(GetOffset)() CONST;
	void METHOD(Destroy)();
	void METHOD(FlushMappedMemory)();
	static void METHOD(GenerateBufferCreateInfo)(VkBufferCreateInfo& buffer_create_info, CONST BufferDesc& desc);
	static ENUM_VulkanAllocationFlags METHOD(TranslateBufferTypeToVulkanAllocationFlags)(CONST ENUM_BUFFER_TYPE& buffer_usage);
		// VK_Evictable overrides
		VIRTUAL Bool METHOD(CanMove)() CONST OVERRIDE;
		VIRTUAL void METHOD(Move)(VK_Device& device, VK_CommandBuffer* cmd, VK_Allocation& new_allocation) OVERRIDE;
		VIRTUAL VkBuffer METHOD(GetVkBuffer)() CONST OVERRIDE;
		VIRTUAL UInt32 METHOD(GetMemorySize)() CONST OVERRIDE;
protected:
	void AllocateMemory();


private:

#pragma endregion


#pragma region MEMBER
public:

protected:
	VK_Device* device = nullptr;
	VkBuffer buffer = VK_NULL_HANDLE;
	VK_Allocation allocation;
	Bool is_dedicated_buffer = false;  // true for non-pooled buffers (Bug D2c)

	enum class LockState : UInt8
	{
		Default,
		Locked,
		Unlocked,
		PersistentMapping
	};
	MYRENDERER_BEGIN_STRUCT(Mapping)
	public:
		VK_Buffer* map_staging_buffer = nullptr;
		ENUM_MAP_TYPE map_type=ENUM_MAP_TYPE::Read;
	MYRENDERER_END_STRUCT

	LockState lock_state = LockState::Default;
	UInt32 lock_counter = 0;
	Mapping mapping;
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_BEGIN_CLASS(VK_StagingBufferManager)

#pragma region METHOD
public:
	VK_StagingBufferManager(VK_Device* in_device);
	VIRTUAL ~VK_StagingBufferManager() ;

	VK_Buffer* METHOD(GetStagingBuffer)(UInt64 size);
	void METHOD(ReleaseStagingBuffer)(VK_Buffer*& buffer, VK_CommandBuffer* command_buffer);

	void METHOD(ProcessPendingFree)(Bool is_immediate, Bool is_free_to_os);
protected:

	MYRENDERER_BEGIN_STRUCT(PendingItemPerCmdBuffer)
		VK_CommandBuffer* command_buffer = nullptr;
		MYRENDERER_BEGIN_STRUCT(PendingItems)
			Vector<VK_Buffer*> buffer;
			UInt64 fence = 0;
		MYRENDERER_END_STRUCT
		Vector<PendingItems> pending_items;
	inline PendingItems* METHOD(FindOrAddItemsForFence)(UInt64 fence);
	MYRENDERER_END_STRUCT

	PendingItemPerCmdBuffer* METHOD(FindOrAddPendingItemPerCmdBuffer)(VK_CommandBuffer* command_buffer);
private:

#pragma endregion


#pragma region MEMBER
public:

protected:
	VK_Device* device;
	UInt64 max_buffer_size = 0;
	UInt64 current_buffer_size = 0;

	Vector<VK_Buffer*> using_buffers;



		Vector<PendingItemPerCmdBuffer> pending_items_per_cmd_buffer;
	MYRENDERER_BEGIN_STRUCT(FreeEntry)
		VK_Buffer* buffer = nullptr;
		UInt64 frame_num = 0;

		Bool operator==(const FreeEntry& rhs) const
		{
			return buffer == rhs.buffer ;
		}
	MYRENDERER_END_STRUCT
		Vector<FreeEntry> free_buffers;


private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif
