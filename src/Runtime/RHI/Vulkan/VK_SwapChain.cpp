#include "VK_SwapChain.h"
#if !PLATFORM_ANDROID
#define  GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <set>
#include<memory>
#include <stdexcept>
#include "VK_Device.h"
#include "VK_Platform.h"
#include "vulkan/vulkan_core.h"
#include "VK_Utils.h"
#include "VK_Define.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

VK_SwapChain::VK_SwapChain(VkInstance in_instance, VK_Device* in_device, void* in_window_handle, CONST ENUM_TEXTURE_FORMAT& in_format, Int width, Int height, Bool is_full_screen,
	UInt32* in_out_desired_num_back_buffers, VkFormat& out_pixel_format, Vector<VkImage>& out_images, Int in_lock_to_vsync, VK_SwapChainRecreateInfo* recreate_info) :
	swapchain(VK_NULL_HANDLE)
	, device(in_device)
	, surface(VK_NULL_HANDLE)
	, window_handle(in_window_handle)
	, current_image_index(-1)
	, semaphore_index(0)
	, num_present_calls(0)
	, num_acquire_calls(0)
	, instance(in_instance)
	, lock_to_vsync(in_lock_to_vsync)
{
	if (recreate_info != nullptr)
	{
		surface = recreate_info->surface;
		recreate_info->surface = VK_NULL_HANDLE;
	} 
	else
	{
		VK_Platform::CreateSurface(window_handle,instance,&surface);
	}
	VkSurfaceCapabilitiesKHR surf_properties;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device->GetGpu(), surface, &surf_properties);

	VkSurfaceTransformFlagBitsKHR pre_transform;
	if (surf_properties.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		pre_transform = surf_properties.currentTransform;
	}
	if ((surf_properties.minImageExtent.width == 0 && surf_properties.maxImageExtent.width == 0) || (surf_properties.minImageExtent.height == 0 && surf_properties.maxImageExtent.height == 0))
	{
		image_extent2D = { 0,0 };
		return;
	}

	UInt32 num_formats=0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device->GetGpu(), surface, &num_formats, nullptr);
	if (num_formats <= 0)
	{
		return;
	}
	std::vector<VkSurfaceFormatKHR> Formats(num_formats);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device->GetGpu(), surface, &num_formats, Formats.data());

	VkColorSpaceKHR requested_colorspace = Formats[0].colorSpace;
	VkSurfaceFormatKHR cur_format = Formats[0];
	VkFormat needed_format = VK_Utils::Translate_Texture_Format_To_Vulkan(in_format);
	VkFormat needed_format2 = needed_format;
	if (needed_format == VK_FORMAT_B8G8R8A8_UNORM)
	{
		needed_format2 = VK_FORMAT_R8G8B8A8_UNORM;
	}
	for (const auto& Format : Formats) {
		if ((Format.format == needed_format || Format.format == needed_format2) && Format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			cur_format = Format;
			requested_colorspace = Format.colorSpace;
			break;
		}
	} 


	width = std::max(std::max(surf_properties.minImageExtent.width, std::min(surf_properties.maxImageExtent.width, (UInt32)width)),1u);
	height = std::max(std::max(surf_properties.minImageExtent.height, std::min(surf_properties.maxImageExtent.height, (UInt32)height)),1u);

	VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
	UInt32 num_found_present_modes = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->GetGpu(), surface, &num_found_present_modes, nullptr);;
	if (num_found_present_modes <= 0)
	{
		return;
	}

	Vector<VkPresentModeKHR> FoundPresentModes(num_found_present_modes);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device->GetGpu(), surface, &num_found_present_modes, FoundPresentModes.data());


	// MAILBOX is NOT used: on this driver (NVIDIA, 2026-08 verified) MAILBOX
	// + uncapped frame rate leaks ~27MB/s of driver host memory (MEM_MAPPED)
	// that never returns — the editor grew to 8+GB and crashed within minutes.
	// FIFO (vsync) is the only verified-stable mode here; revisit when the
	// driver stops leaking or a vsync-aware MAILBOX (flag-gated) is wanted.
	(void)FoundPresentModes;
	VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	if (surf_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
	{
		composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	}


	UInt32 desired_num_buffers = surf_properties.maxImageCount > 0 ? 
		(*in_out_desired_num_back_buffers < surf_properties.minImageCount ? 
			surf_properties.minImageCount : 
			(*in_out_desired_num_back_buffers < surf_properties.maxImageCount ? *in_out_desired_num_back_buffers : surf_properties.maxImageCount)) 
		: *in_out_desired_num_back_buffers;
	VkSwapchainCreateInfoKHR swapchain_info;
	swapchain_info.surface = surface;
	swapchain_info.minImageCount = desired_num_buffers;
	swapchain_info.imageFormat = cur_format.format;
	swapchain_info.imageColorSpace = cur_format.colorSpace;
	swapchain_info.imageExtent.width = width;
	swapchain_info.imageExtent.height = height;
	swapchain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapchain_info.preTransform = pre_transform;
	swapchain_info.imageArrayLayers = 1;
	swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_info.presentMode = present_mode;
	swapchain_info.oldSwapchain = VK_NULL_HANDLE;
	swapchain_info.pNext = nullptr;
	swapchain_info.flags = 0;
	if (recreate_info != nullptr)
	{
		swapchain_info.oldSwapchain = recreate_info->swapchain;
	}

	swapchain_info.clipped = VK_TRUE;
	swapchain_info.compositeAlpha = composite_alpha;
	swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	*in_out_desired_num_back_buffers = desired_num_buffers;

	CHECK_WITH_LOG(vkCreateSwapchainKHR(device->GetDevice(), &swapchain_info, nullptr, &swapchain) != VK_SUCCESS,
					"RHI Error: failed to create swap chain!")

	vkDeviceWaitIdle(device->GetDevice());
	if (recreate_info != nullptr && recreate_info->swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(device->GetDevice(), recreate_info->swapchain, VULKAN_CPU_ALLOCATOR);
		recreate_info->swapchain = VK_NULL_HANDLE;
	}

	vkGetSwapchainImagesKHR(device->GetDevice(), swapchain, &num_swap_chain_images, nullptr);
	out_images.resize(num_swap_chain_images);
	vkGetSwapchainImagesKHR(device->GetDevice(), swapchain, &num_swap_chain_images, out_images.data());
	image_size = std::min(num_swap_chain_images,(UInt32) max_frames_in_flight);
	image_format = cur_format.format;
	image_extent2D = swapchain_info.imageExtent;
	out_pixel_format = image_format;
	if(surface)
	{
		device->CreatePresentQueue(surface);
	}
	CreateSyncObjects();
}


VK_SwapChain::~VK_SwapChain()
{
	//Destroy(nullptr);
}

void VK_SwapChain::Destroy(VK_SwapChainRecreateInfo* RecreateInfo)
{

	bool bRecreate = RecreateInfo != nullptr;
	if (bRecreate)
	{
		RecreateInfo->swapchain = swapchain;
		RecreateInfo->surface = surface;
	}
	else
	{
		vkDeviceWaitIdle(device->GetDevice());
		vkDestroySwapchainKHR(device->GetDevice(), swapchain, nullptr);
		vkDestroySurfaceKHR(instance, surface, nullptr);
	}
	swapchain = VK_NULL_HANDLE;
	surface = VK_NULL_HANDLE;

	for (Int i = 0; i < max_frames_in_flight; i++)
	{
		if (image_available_for_render_semaphore[i])
		{
			vkDestroySemaphore(device->GetDevice(), image_available_for_render_semaphore[i], nullptr);
			image_available_for_render_semaphore[i] = VK_NULL_HANDLE;
		}
		if (image_finished_for_presentation_semaphore[i])
		{
			vkDestroySemaphore(device->GetDevice(), image_finished_for_presentation_semaphore[i], nullptr);
			image_finished_for_presentation_semaphore[i] = VK_NULL_HANDLE;
		}
	}
}

VkFormat VK_SwapChain::GetImageFormat() CONST
{
	return image_format;
}

VkSwapchainKHR& VK_SwapChain::GetSwapchain()
{
	return swapchain;
}

VkExtent2D VK_SwapChain::GetExtent2D() CONST
{
	return image_extent2D;
}

UInt32 VK_SwapChain::GetSwapChainImagesNum() CONST
{
	return num_swap_chain_images;
}

VkSurfaceKHR VK_SwapChain::GetSurface() CONST
{
	return surface;
}

VkInstance VK_SwapChain::GetInstance() CONST
{
	return instance;
}

void VK_SwapChain::CreateSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext= nullptr;
	for (Int i = 0; i < max_frames_in_flight; i++) 
	{
		CHECK_WITH_LOG (vkCreateSemaphore(device->GetDevice(), &semaphoreInfo, nullptr, &image_available_for_render_semaphore[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device->GetDevice(), &semaphoreInfo, nullptr, &image_finished_for_presentation_semaphore[i]) != VK_SUCCESS ,
			"RHI Error: failed to create synchronization objects for a frame!")
	}
	
}

Bool VK_SwapChain::TryGetNextImageIndex(VkSemaphore& out_semaphore, UInt32& out_image_index)
{
	VkResult result = vkAcquireNextImageKHR(device->GetDevice(), swapchain, UINT64_MAX, image_available_for_render_semaphore[current_frame_in_flight], VK_NULL_HANDLE, &out_image_index);
	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		return false;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		CHECK_WITH_LOG(true, "RHI Error: failed to acquire swap chain image!");
	}
	out_semaphore = image_available_for_render_semaphore[current_frame_in_flight];
	return true;
}

VkResult VK_SwapChain::PresentInternal(VkQueue present_queue, VkSemaphore wait_semaphore, CONST UInt32& image_index, Bool is_lock_to_vsync)
{
	VkResult result = VK_SUCCESS;
	VkPresentInfoKHR present_info = {};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &swapchain;
	present_info.pImageIndices = &image_index;
	present_info.pResults = nullptr;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &wait_semaphore;
	present_info.pResults = &result;
	present_info.pNext = nullptr;

	vkQueuePresentKHR(present_queue, &present_info);

	current_frame_in_flight = (current_frame_in_flight + 1) % image_size;
	return result;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

