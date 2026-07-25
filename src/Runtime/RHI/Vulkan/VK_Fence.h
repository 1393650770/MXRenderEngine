#pragma once
#ifndef _VK_FENCE_
#define _VK_FENCE_
#include <vulkan/vulkan_core.h>

#include "Core/ConstDefine.h"
#include "RHI/RenderResource.h"




MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)
class VK_Device;
class VK_FenceManager;
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VK_Fence,public RenderResource)

#pragma region METHOD
public:
    VK_Fence(VK_Device* in_device,VK_FenceManager* in_fence_manager, Bool is_create_signaled);
    VIRTUAL~VK_Fence();
    VkFence METHOD(GetFence)() CONST;
    VK_FenceManager* METHOD(GetOwner)() CONST;
    Bool METHOD(GetIsSignaled)() CONST;
    Bool METHOD(CheckSignaled)();
    void METHOD(ResetFence)();
protected:
    
private:

#pragma endregion

#pragma region MEMBER
public:

protected:
    VK_Device* device;
    VK_FenceManager* owner_fence_manager;
    VkFence fence = VK_NULL_HANDLE;
	enum class ENUM_Fence_State : UInt8
	{
		NotReady,
		Signaled,
	};
    ENUM_Fence_State state;

private:

#pragma endregion
    friend class VK_FenceManager;
MYRENDERER_END_CLASS

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VK_FenceManager,public RenderResource)
#pragma region METHOD
public:
    VK_FenceManager(VK_Device* in_device);
    VIRTUAL~VK_FenceManager();

    VK_Fence* METHOD(GetOrCreateFence)();
    void METHOD(DestroyManagerResource)();
    void METHOD(FreeFence)(VK_Fence*& fence);
    void METHOD(ResetFence)(VK_Fence* fence);
    Bool METHOD(WaitForFence)(VK_Fence* fence, UInt64 time_in_nanoseconds);

protected:
    
private:

#pragma endregion

#pragma region MEMBER
public:

protected:
    VK_Device* device;
    Vector<VK_Fence*> free_fences;
    Vector<VK_Fence*> using_fences;

private:

#pragma endregion


MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif //_VK_QUEUE_
