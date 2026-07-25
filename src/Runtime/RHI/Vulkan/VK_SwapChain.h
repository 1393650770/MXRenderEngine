#pragma once

#ifndef _VK_SWAPCHAIN_
#define _VK_SWAPCHAIN_

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include "RHI/RenderResource.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

class VK_Device;

MYRENDERER_BEGIN_STRUCT(VK_SwapChainRecreateInfo)
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkSurfaceKHR surface{};
MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VK_SwapChain,public RenderResource)

#pragma region METHOD
public:
	VK_SwapChain(VkInstance in_instance, VK_Device* in_device, void* in_window_handle,CONST ENUM_TEXTURE_FORMAT& in_format, Int width, Int height, Bool is_full_screen,
		UInt32* in_out_desired_num_back_buffers, VkFormat& out_pixel_format, Vector<VkImage>& out_images, Int in_lock_to_vsync, VK_SwapChainRecreateInfo* recreate_info);
	VIRTUAL ~VK_SwapChain();

	void METHOD(Destroy)(VK_SwapChainRecreateInfo* RecreateInfo);

	VkFormat METHOD(GetImageFormat)() CONST;

	VkSwapchainKHR& METHOD(GetSwapchain)();
	VkExtent2D METHOD(GetExtent2D)() CONST;
	UInt32 METHOD(GetSwapChainImagesNum)() CONST;
	VkSurfaceKHR METHOD(GetSurface)() CONST;
	VkInstance METHOD(GetInstance)() CONST;
	VkResult METHOD(PresentInternal)(VkQueue present_queue, VkSemaphore wait_semaphore, CONST UInt32& image_index, Bool is_lock_to_vsync);
	Bool METHOD(TryGetNextImageIndex)(VkSemaphore& semaphore, UInt32& out_image_index);
protected:
	void METHOD(CreateSyncObjects)();
#pragma endregion


#pragma region MEMBER

private:
protected:
	VkSurfaceTransformFlagBitsKHR qcom_render_pass_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

	VkFormat image_format = VK_FORMAT_UNDEFINED;
	VkExtent2D image_extent2D;
	VkSwapchainKHR swapchain;
	VK_Device* device;

	VkSurfaceKHR surface;
	void* window_handle;

	Int current_image_index;
	Int semaphore_index;
	Int num_present_calls;
	Int num_acquire_calls;
	Int internal_width = 0;
	Int internal_height = 0;
	Bool b_internal_fullscreen = false;

	Int RT_pacing_sample_count = 0;

	Float64 rt_pacing_previous_frameCPUtime = 0;
	Float64 rt_pacing_sampled_deltatimeMS = 0;

	Float64 next_present_target_time = 0;

	VkInstance instance;

	Int lock_to_vsync;

	Int present_ID = 0;
	UInt32 num_swap_chain_images;

	static UInt8 CONST   max_frames_in_flight{ 3 };
	UInt8 image_size = 3;
	VkSemaphore          image_available_for_render_semaphore[max_frames_in_flight]{VK_NULL_HANDLE};
	VkSemaphore          image_finished_for_presentation_semaphore[max_frames_in_flight]{ VK_NULL_HANDLE };
	UInt8                current_frame_in_flight = 0;
#pragma endregion





MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif //
