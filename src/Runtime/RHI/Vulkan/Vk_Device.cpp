#include "VK_Device.h"
#include <iostream>
#include <utility>

#include "VK_Fence.h"
#include "VK_Queue.h"
#include "VK_Memory.h"
#include "VK_Define.h"
#include "VK_CommandBuffer.h"
#include "VK_Buffer.h"
#include "VK_RenderPass.h"
#include "VK_PipelineState.h"
#include "VK_FrameBuffer.h"
#include "VK_DescriptorSets.h"
#include "VK_Extension.h"
#include "VK_ResourcePool.h"
#include "VK_BindlessManager.h"
#include "RHI/ResourceManager.h"
#include "Render/Core/RenderGraphResource.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

QueueFamilyIndices VK_Device::FindQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;

	UInt32 queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

	Vector<VkQueueFamilyProperties> queueFamilies(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queueFamilies.data());

	Int i = 0;
	for (CONST auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphics_family = i;
		}
		if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
			indices.compute_family = i;
		}
		if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
			indices.transfer_family = i;
		}

		//VkBool32 presentSupport = false;
		//vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

		//if (presentSupport) {
		//	indices.presentFamily = i;
		//}

		if (indices.isComplete()) {
			break;
		}

		i++;
	}

	return indices;
}

void VK_Device::CreateDevice(Bool enable_validation_layers, CONST Vector<UniquePtr<VK_Extension>>& enable_feature, Vector<CONST Char*> device_extensions, Vector<CONST Char*> validation_layers)
{
	queue_family_indices = FindQueueFamilies(gpu);

	Vector<VkDeviceQueueCreateInfo> queue_create_infos;
	Set<UInt32> unique_queue_families = { queue_family_indices.graphics_family.value(), queue_family_indices.compute_family.value() };

	float queuePriority = 1.0f;
	for (UInt32   queueFamily : unique_queue_families) {
		VkDeviceQueueCreateInfo queue_create_info{};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queueFamily;
		queue_create_info.queueCount = 1;
		queue_create_info.pQueuePriorities = &queuePriority;
		queue_create_infos.push_back(queue_create_info);
	}


	VkDeviceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

	create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	create_info.pQueueCreateInfos = queue_create_infos.data();

	create_info.pEnabledFeatures = &gpu_features.core_1_0;

	for (auto& extension : enable_feature)
	{
		if (extension->GetIsInUse())
		{
			extension->PreCreateDevice(create_info);
			device_extensions.push_back(extension->GetName().c_str());
		}
	}

	// Vulkan 1.1 core feature: `shaderDrawParameters`.
	//
	// Without it DrawIndexedIndirectArgs::firstInstance is ignored and
	// gl_InstanceIndex restarts at 0 for every indirect command, which forces
	// one draw call per batch plus a push constant carrying the batch offset.
	// With it, every batch can be issued as a single vkCmdDrawIndexedIndirect
	// of drawCount = N, because gl_InstanceIndex then includes firstInstance.
	//
	// Only shaderDrawParameters is enabled; the rest of core_1_1 stays off. No
	// standalone 1.1 extension struct (multiview, variable_pointers, ...) is
	// chained anywhere in this engine, so this does not trip
	// VUID-VkDeviceCreateInfo-pNext-02830. Chained before core_1_3 below, which
	// prepends itself and therefore ends up linking to this one.
	VkPhysicalDeviceVulkan11Features feat_1_1{};
	feat_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	feat_1_1.pNext = nullptr;
	if (api_version >= VK_API_VERSION_1_1 && gpu_features.core_1_1.shaderDrawParameters == VK_TRUE)
	{
		feat_1_1.shaderDrawParameters = VK_TRUE;
		feat_1_1.pNext = const_cast<void*>(create_info.pNext);
		create_info.pNext = &feat_1_1;
	}

	// Prepend Vulkan 1.3 physical-device features (dynamicRendering, synchronization2)
	// to the pNext chain. Only chain core_1_3 — core_1_1 and core_1_2 features are
	// already provided by individual extension structs (DescriptorIndexing, BufferDeviceAddress
	// etc.), and adding both the per-extension struct AND the bundled VkPhysicalDeviceVulkan12Features
	// violates VUID-VkDeviceCreateInfo-pNext-02830.
	if (api_version >= VK_API_VERSION_1_3)
	{
		gpu_features.core_1_3.pNext = const_cast<void*>(create_info.pNext);
		create_info.pNext = &gpu_features.core_1_3;
	}

	create_info.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
	create_info.ppEnabledExtensionNames = device_extensions.data();

	// Device layers are deprecated since Vulkan 1.0; only instance layers are valid.
	create_info.enabledLayerCount = 0;

	CHECK_WITH_LOG(vkCreateDevice(gpu, &create_info, nullptr, &(device)) != VK_SUCCESS,"RHI Error: failed to create logical device!");
	CreateQueue(queue_family_indices);
}



void VK_Device::GetDeviceExtensionsAndLayers(VkPhysicalDevice Gpu, UInt32 VendorId, Vector<CONST Char*>& OutDeviceExtensions, Vector<CONST Char*>& OutDeviceLayers, Vector<String>& OutAllDeviceExtensions, Vector<String>& OutAllDeviceLayers, Bool& bOutDebugMarkers)
{


}
void VK_Device::CreateQueue(QueueFamilyIndices family_indice)
{
	graph_queue=new VK_Queue(this,family_indice.graphics_family.value());
	compute_queue = new VK_Queue(this, family_indice.compute_family.value());
	//present_queue = new VK_Queue(this, family_indice.presentFamily.value());
}

void VK_Device::Init(Int device_index,Bool enable_validation_layers,CONST Vector<UniquePtr<VK_Extension>>& enable_feature, Vector<CONST Char*> device_extensions, Vector<CONST Char*> validation_layers)
{
	CreateDevice(enable_validation_layers, enable_feature, std::move(device_extensions), std::move(validation_layers));

	// Verify optional features via vkGetInstanceProcAddr (more reliable than apiVersion alone).
	if (api_version >= VK_API_VERSION_1_3 && vkGetDeviceProcAddr(device, "vkCmdBeginRendering"))
		extensions.HasKHRDynamicRendering = 1;
	if (api_version >= VK_API_VERSION_1_3 && vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier2"))
		extensions.HasKHRSynchronization2 = 1;

	device_memory_manager=new VK_DeviceMemoryManager(this);
	memory_manager=new VK_MemoryManager(this);
	fence_manager=new VK_FenceManager(this);
	command_buffer_manager=new VK_CommandBufferManager(this);
	staging_buffer_manager = new VK_StagingBufferManager(this);
	render_pass_manager = new VK_RenderPassManager(this);
	pipeline_state_manager = new VK_PipelineStateManager(this);
	frame_buffer_manager = new VK_FrameBufferManager(this);
	descriptset_allocator = new VK_DescriptsetAllocator(this, gpu_props.limits.maxSamplerAllocationCount);
	bindless_manager = new VK_BindlessManager(this);
	resource_pool = new VK_ResourcePool();
	MXRender::Render::g_resource_pool = resource_pool;
	resource_manager = new MXRender::RHI::ResourceManager();
	MXRender::RHI::g_resource_manager = resource_manager;

	// --   Set global device handle for RenderDoc debug object names
	extern VkDevice g_debug_name_device;
	g_debug_name_device = device;
}

VkDevice VK_Device::GetDevice()
{
	return device;
}

VkPhysicalDevice VK_Device::GetGpu()
{
	return gpu;
}

VK_FenceManager* VK_Device::GetFenceManager()
{
	return fence_manager;
}

VK_DeviceMemoryManager* VK_Device::GetDeviceMemoryManager()
{
	return device_memory_manager;
}

VK_MemoryManager* VK_Device::GetMemoryManager()
{
	return memory_manager;
}

VK_StagingBufferManager* VK_Device::GetStagingBufferManager()
{
	return staging_buffer_manager;
}

VK_ResourcePool* VK_Device::GetResourcePool()
{
	return resource_pool;
}

VK_CommandBufferManager* VK_Device::GetCommandBufferManager()
{
	return command_buffer_manager;
}

VK_Queue* VK_Device::GetQueue(ENUM_QUEUE_TYPE queue_type)
{
	switch (queue_type)
	{
	case ENUM_QUEUE_TYPE::GRAPHICS:
		return graph_queue;
	case ENUM_QUEUE_TYPE::COMPUTE:
		return compute_queue == nullptr? graph_queue : compute_queue;
	case ENUM_QUEUE_TYPE::TRANSFER:
		return transfer_queue==nullptr? graph_queue : transfer_queue;
	case ENUM_QUEUE_TYPE::PRESENT:
		return present_queue == nullptr ? graph_queue : present_queue;
	default:
		return nullptr;
	}
	return nullptr;
}

CONST OptionalVulkanDeviceExtensions& VK_Device::GetOptionalExtensions() CONST
{
	return extensions;
}

void VK_Device::CreatePresentQueue(VkSurfaceKHR surface)
{
	if(!present_queue)
	{
		auto check_and_create_queue=[&](VK_Queue* queue) ->Bool
		{
			VkBool32 present_support = false;
			if(queue)
			{
				vkGetPhysicalDeviceSurfaceSupportKHR(gpu, queue->GetFamily(), surface, &present_support);
				if (present_support&& !present_queue) {
					present_queue = new VK_Queue(this, graph_queue->GetFamily());
				}
			}
			return present_support;
		};

		CHECK_WITH_LOG(!(check_and_create_queue(graph_queue) || check_and_create_queue(compute_queue)||check_and_create_queue(transfer_queue)),
						"RHI Error: failed to create present queue!")
	}
}

CONST VkPhysicalDeviceLimits& VK_Device::GetLimits() CONST
{
	return gpu_props.limits;
}

QueueFamilyIndices VK_Device::GetQueueFamilyIndices() CONST
{
	return queue_family_indices;
}

VK_Device::~VK_Device()
{
	if(device)
	{ 
		Destroy();
		device=VK_NULL_HANDLE;
	}
}

VK_Device::VK_Device(VulkanRHI* in_vulkan_rhi, VkPhysicalDevice in_gpu):
	gpu(in_gpu),
	vulkan_rhi(in_vulkan_rhi)
{
	vkGetPhysicalDeviceProperties(gpu,&gpu_props);
	vendor_id=gpu_props.vendorID;
	api_version = gpu_props.apiVersion;

	gpu_features.Query(gpu, api_version);
}

void VK_Device::Destroy()
{
	if (graph_queue)
	{
		delete graph_queue;
		graph_queue=nullptr;
	}
	if (compute_queue)
	{
		delete compute_queue;
		compute_queue=nullptr;
	}
	if (transfer_queue)
	{
		delete transfer_queue;
		transfer_queue=nullptr;
	}
	if (present_queue)
	{
		delete present_queue;
		present_queue=nullptr;
	}
	if (staging_buffer_manager)
	{
		delete staging_buffer_manager;
		staging_buffer_manager = nullptr;
	}
	if (resource_pool)
	{
		MXRender::Render::g_resource_pool = nullptr;
		delete resource_pool;
		resource_pool = nullptr;
	}
	if (memory_manager)
	{
		delete memory_manager;
		memory_manager = nullptr;
	}
	if (device_memory_manager)
	{
		delete device_memory_manager;
		device_memory_manager = nullptr;
	}
	if (command_buffer_manager)
	{
		delete command_buffer_manager;
		command_buffer_manager=nullptr;
	}
	if (fence_manager)
	{
		delete fence_manager;
		fence_manager = nullptr;
	}
	if (render_pass_manager)
	{
		delete render_pass_manager;
		render_pass_manager=nullptr;
	}
	if (pipeline_state_manager)
	{
		delete pipeline_state_manager;
		pipeline_state_manager=nullptr;
	}
	if (frame_buffer_manager)
	{
		delete frame_buffer_manager;
		frame_buffer_manager=nullptr;
	}
	if (descriptset_allocator)
	{
		delete descriptset_allocator;
		descriptset_allocator=nullptr;
	}
	if (bindless_manager)
	{
		delete bindless_manager;
		bindless_manager = nullptr;
		}
		if (resource_manager)
		{
		MXRender::RHI::g_resource_manager = nullptr;
		delete resource_manager;
		resource_manager = nullptr;
		}
		if (resource_pool)
		{
			MXRender::Render::g_resource_pool = nullptr;
			delete resource_pool;
			resource_pool = nullptr;
		}
	if (device)
	{
		vkDestroyDevice(device, VULKAN_CPU_ALLOCATOR);
		device=VK_NULL_HANDLE;
	}	
}

VK_RenderPassManager* VK_Device::GetRenderPassManager()
{
	return render_pass_manager;
}

VK_PipelineStateManager* VK_Device::GetPipelineStateManager()
{
	return pipeline_state_manager;
}

VK_FrameBufferManager* VK_Device::GetFrameBufferManager()
{
	return frame_buffer_manager;
}

VK_DescriptsetAllocator* VK_Device::GetDescriptsetAllocator()
{
	return descriptset_allocator;
}

VK_BindlessManager* VK_Device::GetBindlessManager()
{
	return bindless_manager;
}

Vector<UniquePtr<MXRender::RHI::Vulkan::VK_Extension>> VK_Device::EnableDefaultFeature()
{
	auto support_extensions= VK_Extension::GetRenderSupportGpuFeatures(this, this->api_version);
	VkPhysicalDeviceFeatures2 physical_device_features2;
	physical_device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	physical_device_features2.pNext = nullptr;
	for (auto& extension : support_extensions)
	{
		if (extension->GetIsInUse())
		{
			extension->PrePhysicalDeviceFeatures(physical_device_features2);
		}
	}
	vkGetPhysicalDeviceFeatures2(gpu, &physical_device_features2);
	for (auto& extension : support_extensions)
	{
		if (extension->GetIsInUse())
		{
			extension->PostPhysicalDeviceFeatures(&extensions);
		}
	}
	VkPhysicalDeviceProperties2 physical_device_properties2;
	physical_device_properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physical_device_properties2.pNext = nullptr;
	//physical_device_properties2.pNext = &GpuIdProps;
	for (auto& extension : support_extensions)
	{
		if (extension->GetIsInUse())
		{
			extension->PrePhysicalDeviceProperties(physical_device_properties2);
		}
	}
	vkGetPhysicalDeviceProperties2(gpu, &physical_device_properties2);
	for (auto& extension : support_extensions)
	{
		if (extension->GetIsInUse())
		{
			extension->PostPhysicalDeviceProperties();
		}
	}
	Vector<VkExtensionProperties> device_extensions;
	UInt32 count = 0;
	vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, nullptr);
	if (count > 0)
	{
		device_extensions.resize(count);
		vkEnumerateDeviceExtensionProperties(gpu, nullptr, &count, device_extensions.data());
	}
	for (CONST VkExtensionProperties& device_extension : device_extensions)
	{
		CONST Int extension_index = VK_Extension::FindExtension(support_extensions, device_extension.extensionName);
		CONST Bool is_found = (extension_index != -1);
		Bool is_core = false;
		if (is_found)
		{
			support_extensions[extension_index]->SetIsSupported();
			is_core = support_extensions[extension_index]->SetIsCore(api_version);
		}
	}
	return std::move(support_extensions);
}

CONST VK_DeviceFeatureProperties& VK_Device::GetOptionalExtensionProperties() CONST
{
	return gpu_feature_propertis;
}

void VK_DeviceFeature::Query(VkPhysicalDevice gpu, UInt32 api_version)
{
	VkPhysicalDeviceFeatures2 physical_device_features2;
	physical_device_features2.sType= VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	physical_device_features2.pNext = &core_1_1;
	core_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	core_1_1.pNext = nullptr;
	if (api_version >= VK_API_VERSION_1_2)
	{
		core_1_1.pNext = &core_1_2;
		core_1_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		core_1_2.pNext = nullptr;
	}

	if (api_version >= VK_API_VERSION_1_3)
	{
		core_1_2.pNext = &core_1_3;
		core_1_3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		core_1_3.pNext = nullptr;
	}

	vkGetPhysicalDeviceFeatures2(gpu, &physical_device_features2);

	// Copy features into old struct for convenience
	core_1_0 = physical_device_features2.features;

	// Apply config modifications
	//core_1_0.robustBufferAccess = GCVarRobustBufferAccess.GetValueOnAnyThread() > 0 ? VK_TRUE : VK_FALSE;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE