
#include "VK_RenderRHI.h"

#include "VK_Device.h"
#if !PLATFORM_ANDROID
#define  GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#endif
#include "VK_Buffer.h"
#include "VK_Viewport.h"
#include "VK_Shader.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include "Platform/PlatformDebug.h"
#include <thread>
#include "VK_Texture.h"
#include "VK_RenderPass.h"
#include "VK_FrameBuffer.h"
#include "VK_PipelineState.h"
#include "VK_CommandBuffer.h"
#include "VK_Queue.h"
#include "VK_BindlessManager.h"
#include "vulkan/vulkan_core.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

CONST Vector<CONST Char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

// Hard-required: only swapchain. descriptor_indexing probed via optional VK_Extension path.
Vector<CONST Char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

//  UE  VULKAN_REPORT_LOG — route validation messages through PlatformDebug
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	Platform::DebugPrintf("VulkanValidation", "%s", pCallbackData->pMessage);
	// Only abort on ERROR-level validation messages on non-Android (UE behaviour: never abort)
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		Platform::DebugErrorf("VulkanValidation", "%s", pCallbackData->pMessage);
	}
	return VK_FALSE;
}

void VulkanRHI::Init(RenderFactory* render_factory)
{
	RenderFactory* vulkan_render_factory = STATIC_CAST(render_factory, RenderFactory);
		// Backward compat: if old enable_render_debug is true but new validation_level is 0, enable level 2
			Int validation_lvl = vulkan_render_factory->validation_level;
			if (validation_lvl == 0 && vulkan_render_factory->enable_render_debug)
				validation_lvl = 2;
			Bool enable_validation = (validation_lvl > 0);
			Bool enable_debug_cb = vulkan_render_factory->enable_debug_callback;
			Bool validation_optional = vulkan_render_factory->validation_optional;
		//  从 factory 读取线程模式
		g_thread_mode = vulkan_render_factory->threading_mode;
		// Editor forces Single mode: its ImGui chain (NewFrame -> record ->
		// RHI replay) touches ImGui's Vulkan backend across threads and the
		// draw-data buffers are reused by the next NewFrame. Single mode
		// executes commands inline, which is the safe (and sample-like) path.
		const char* env = std::getenv("MX_THREAD_MODE");
		if (env)
		{
			if (std::strcmp(env, "single") == 0)
				g_thread_mode = EThreadingMode::Single;
			else if (std::strcmp(env, "threethread") == 0)
				g_thread_mode = EThreadingMode::ThreeThread;
		}
		g_enable_rhi_thread = (g_thread_mode >= EThreadingMode::RHIThread);
		CreateInstance(enable_validation, validation_optional);
		InitializeDebugmessenger(enable_debug_cb);
		CreateDevice(enable_validation);
	write_cb = device->GetCommandBufferManager()->GetOrCreateCommandBuffer(ENUM_QUEUE_TYPE::GRAPHICS);
	rhi_cb  = device->GetCommandBufferManager()->GetOrCreateCommandBuffer(ENUM_QUEUE_TYPE::GRAPHICS);
	immediate_command_buffer = write_cb;
}

void VulkanRHI::PostInit()
{

}


void VulkanRHI::Shutdown()
{

	if (viewports.size() > 0)
	{
		for (auto& viewport : viewports)
		{
			delete viewport;
		}
		viewports.clear();
	}
	if (defered_command_buffers.size() > 0)
	{
		for (auto& command_buffer : defered_command_buffers)
		{
			delete command_buffer;
		}
		defered_command_buffers.clear();
	}
	if (debug_messenger != VK_NULL_HANDLE)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debug_messenger, nullptr);
		}
		debug_messenger = VK_NULL_HANDLE;
	}
	if (device != nullptr)
	{
		delete device;
		device = nullptr;
	}
	if (instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}
}


Buffer* VulkanRHI::CreateBuffer(const BufferDesc& buffer_desc)
{
	return new VK_Buffer(device,buffer_desc);
}

Texture* VulkanRHI::CreateTexture(const TextureDesc& texture_desc)
{
	return new VK_Texture(device,texture_desc);
}

RenderPipelineState* VulkanRHI::CreateRenderPipelineState(CONST RenderGraphiPipelineStateDesc& desc)
{
	// Compute-only pipeline: no render targets, no depth stencil, no render pass needed
	Bool is_compute = (desc.shaders[ENUM_SHADER_STAGE::Shader_Compute] != nullptr
					&& desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] == nullptr
					&& desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] == nullptr);
	if (is_compute)
	{
		return device->GetPipelineStateManager()->GetPipelineState(desc, nullptr);
	}

	Vector<ENUM_TEXTURE_FORMAT> rtv_formats;
	for (auto& rtv : desc.render_targets)
	{
		rtv_formats.push_back(rtv->GetTextureDesc().format);
	}

		// Dynamic rendering: PSO must be created with VK_NULL_HANDLE render pass,
		// with attachment formats passed via VkPipelineRenderingCreateInfo in pNext.
		if (device->GetOptionalExtensions().HasKHRDynamicRendering)
		{
			return device->GetPipelineStateManager()->GetPipelineState(desc, nullptr);
		}
	RenderPassCacheKey key(desc.render_targets.size(), rtv_formats.data(), desc.depth_stencil_view->GetTextureDesc().format, desc.depth_stencil_view->GetTextureDesc().samples, false, false);
	return device->GetPipelineStateManager()->GetPipelineState(desc, device->GetRenderPassManager()->GetRenderPass(key));
}

// --  
ComputePipelineState* VulkanRHI::CreateComputePipelineState(CONST ComputePipelineStateDesc& desc)
{
	return device->GetPipelineStateManager()->GetComputePipelineState(desc);
}
RenderPass* VulkanRHI::CreateRenderPass(CONST RenderPassDesc& desc)
{
	return device->GetRenderPassManager()->GetRenderPass(desc);
}
FrameBuffer* VulkanRHI::CreateFrameBuffer(CONST FrameBufferDesc& desc)
{
	Vector<ENUM_TEXTURE_FORMAT> rtv_formats;
	for (auto& rtv : desc.render_targets)
	{
		rtv_formats.push_back(rtv->GetTextureDesc().format);
	}
	RenderPassCacheKey key(desc.render_targets.size(), rtv_formats.data(), desc.depth_stencil_view->GetTextureDesc().format, desc.depth_stencil_view->GetTextureDesc().samples, false, false);
	return new VK_FrameBuffer(device,desc, device->GetRenderPassManager()->GetRenderPass(key)->GetRenderPass());
}

// UE-style shadow-memory lock tracker for MapBuffer/UnmapBuffer in record mode
static Map<Buffer*, Vector<UInt8>> s_lock_tracker;

void* VulkanRHI::MapBuffer(Buffer* buffer, ENUM_MAP_TYPE map_type, ENUM_MAP_FLAG map_flag)
{
	auto* cmd_list = STATIC_CAST(GetImmediateCommandList(), VK_CommandBuffer);
	if (cmd_list && !cmd_list->IsBypass())
	{
		// Record mode: allocate CPU shadow memory. The actual GPU Map happens
		// during UnmapBuffer replay on the RHI thread (UE EnqueueLambda pattern).
		Vector<UInt8>& shadow = s_lock_tracker[buffer];
		shadow.resize(STATIC_CAST(buffer, VK_Buffer)->GetBufferDesc().size);
		return shadow.data();
	}
	// Bypass mode: respect caller's map_flag.
	// Discard orphans old allocation (safe for pre-allocation), None reuses the
	// same sub-allocation (safe for per-frame uploads on pre-bound descriptors,
	// per UE's PersistentMapping pattern — offset unchanged, descriptor valid).
	return STATIC_CAST(buffer, VK_Buffer)->Map(map_type, map_flag);
}

void VulkanRHI::UnmapBuffer(Buffer* buffer)
{
	auto* cmd_list = STATIC_CAST(GetImmediateCommandList(), VK_CommandBuffer);
	if (cmd_list && !cmd_list->IsBypass())
	{
		// Record mode: enqueue the GPU upload command
		// RHI thread replays: Map(Discard) → memcpy(shadow→GPU) → Unmap
		auto it = s_lock_tracker.find(buffer);
		if (it != s_lock_tracker.end())
		{
			cmd_list->GetRecordedCommands().push_back(
				std::make_unique<RHICmdUpdateBuffer>(buffer, 0, (UInt32)it->second.size(), it->second.data()));
			s_lock_tracker.erase(it);
		}
		return;
	}
	STATIC_CAST(buffer, VK_Buffer)->Unmap();
}

Bool VulkanRHI::CheckGpuSuitable(VkPhysicalDevice gpu)
{
	//TODO: Check Queue Family Suitable
	
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, availableExtensions.data());

	Set<String> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (CONST auto& extension : availableExtensions) {
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}


VkPhysicalDevice VulkanRHI::GetGpuFromHarddrive()
{
	VkPhysicalDevice gpu=VK_NULL_HANDLE;
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

	if (device_count == 0) {
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> gpus(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, gpus.data());

	for (const auto& cur_gpu : gpus) {
		if (CheckGpuSuitable(cur_gpu)) {
			gpu= cur_gpu;
			break;
		}
	}
	CHECK_WITH_LOG(gpu==VK_NULL_HANDLE,"RHI Error: failed to find a suitable GPU!");
	return gpu;
}

void VulkanRHI::CreateDevice(Bool enable_validation_layers)
{
	if(device==nullptr)
	{
		VkPhysicalDevice physicalDevice =  GetGpuFromHarddrive();
		CHECK_WITH_LOG(physicalDevice==VK_NULL_HANDLE,"RHI Error: failed to find a suitable GPU!");
		device= new  VK_Device(this,physicalDevice);
		auto enable_feature= device->EnableDefaultFeature();
		device->Init(0,enable_validation_layers, enable_feature,deviceExtensions,validationLayers);
	}
}


Vector<CONST Char*> VulkanRHI::GetRequiredExtensions(Bool enable_validation_layers)
{
#if PLATFORM_ANDROID
	std::vector<CONST Char*> extensions = {
		"VK_KHR_surface",
		"VK_KHR_android_surface"
	};
#else
	uint32_t glfwExtensionCount = 0;
	CONST Char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<CONST Char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#endif

	if (enable_validation_layers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	return extensions;
}

Bool VulkanRHI::CheckValidationlayerSupport()
{
	UInt32 layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	Vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (CONST Char* layerName : validationLayers) {
		Bool layerFound = false;

		for (CONST auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}

		if (!layerFound) {
			return false;
		}
	}

	return true;
}

void VulkanRHI::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& create_info)
{
	create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = debugCallback;
}

void VulkanRHI::CreateInstance(Bool enable_validation_layers, Bool validation_optional)
{
		if (enable_validation_layers && !CheckValidationlayerSupport()) {
			if (validation_optional) {
				Platform::DebugWarnf("MXRender", "Vulkan validation layers requested but not available, continuing without them");
				enable_validation_layers = false;
			} else {
				throw std::runtime_error("validation layers requested, but not available!");
			}
		}

	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "MxRender";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "MxRender";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	// Query the highest Vulkan instance version supported by the loader.
		// Previously hardcoded to VK_API_VERSION_1_0 which caused 1.3 functions
		// to be dispatched to no-op stubs when validation layers are absent.
		uint32_t instance_version = VK_API_VERSION_1_0;
		{
			PFN_vkEnumerateInstanceVersion pfn =
				(PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");
			if (pfn) pfn(&instance_version);
		}
		app_info.apiVersion = instance_version;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &app_info;

	auto extensions = GetRequiredExtensions(enable_validation_layers);
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
	if (enable_validation_layers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		PopulateDebugMessengerCreateInfo(debug_create_info);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
	}
	else {
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}
}

void VulkanRHI::InitializeDebugmessenger(Bool enable_validation_layers)
{
	if (!enable_validation_layers) 
		return;

	VkDebugUtilsMessengerCreateInfoEXT create_info;
	PopulateDebugMessengerCreateInfo(create_info);

	if (CreateDebugUtilsMessengerEXT(instance, &create_info, nullptr, &debug_messenger) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

VkResult VulkanRHI::CreateDebugUtilsMessengerEXT(VkInstance instance, CONST VkDebugUtilsMessengerCreateInfoEXT* create_info, CONST VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* debug_messenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, create_info, allocator, debug_messenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

Viewport* VulkanRHI::CreateViewport(void* window_handle, Int width, Int height, Bool is_full_screen)
{
	return new VK_Viewport(this, device, window_handle, width, height, is_full_screen,ENUM_TEXTURE_FORMAT::BGRA8);
}

Shader* VulkanRHI::CreateShader(CONST ShaderDesc& desc, CONST ShaderDataPayload& data)
{
	return new VK_Shader(device,desc,data);
}

CommandList* VulkanRHI::GetImmediateCommandList()
{
	// In bypass mode, Begin() starts the CB directly on main thread.
	if (!g_enable_rhi_thread)
	{
		write_cb->Begin();
	}
	return immediate_command_buffer;
}

CommandList* VulkanRHI::GetWriteCommandList()
{
	// In RHI mode, just return write CB — Begin() will be recorded as command
	return write_cb;
}

CommandList* VulkanRHI::GetRHICmdListForPresent()
{
	// Return the CB that the RHI thread has (or will) replay
	return rhi_cb;
}

void VulkanRHI::SwapCommandLists()
{
	// Stamp the frame onto the JUST-RECORDED write_cb BEFORE swapping it to
	// rhi_cb - the RHI thread reports this stamp back after replay so the
	// render thread can wait for ITS frame (a shared bool would signal the
	// previous frame's completion -> present submits never-replayed
	// commands -> uploads/dispatches silently dropped, e.g. explosion
	// edits never reaching the GPU).
	swap_frame_.fetch_add(1, std::memory_order_acq_rel);
	write_cb->SetRecordedFrame(swap_frame_.load(std::memory_order_acquire));

	// Swap: the just-recorded write_cb becomes rhi_cb for replay+present
	std::swap(write_cb, rhi_cb);
	// Sync immediate_command_buffer so RenderGraph::Execute uses correct CB
	immediate_command_buffer = write_cb;
	// Signal RHI thread
	replay_done.store(false, std::memory_order_release);
	replay_ready.store(true, std::memory_order_release);
}

Bool VulkanRHI::IsReplayDone() CONST
{
	return replay_done.load(std::memory_order_acquire);
}

void VulkanRHI::StartRHIThread()
{
	rhi_running.store(true, std::memory_order_release);
	rhi_thread = std::thread([this]() {
		while (rhi_running.load(std::memory_order_acquire)) {
			if (replay_ready.load(std::memory_order_acquire)) {
				rhi_cb->Replay();
				rhi_cb->End();  // End CB on RHI thread (same thread as Begin during Replay)
				replay_ready.store(false, std::memory_order_release);
				replay_done.store(true, std::memory_order_release);
				// Record WHICH frame finished replaying: the stamp of the
				// command buffer we actually replayed (it may lag several
				// swaps behind the latest one).
				replay_frame_.store(rhi_cb->GetRecordedFrame(), std::memory_order_release);
			} else {
				std::this_thread::yield();
			}
		}
	});
}

Bool VulkanRHI::CheckAndProcessReplay()
{
	if (replay_ready.load(std::memory_order_acquire))
	{
		rhi_cb->Replay();
		replay_ready.store(false, std::memory_order_release);
		replay_done.store(true, std::memory_order_release);
		return true;
	}
	return false;
}

void VulkanRHI::StopRHIThread()
{
	rhi_running.store(false, std::memory_order_release);
	if (rhi_thread.joinable()) {
		rhi_thread.join();
	}
}

// --
CommandList* VulkanRHI::GetCommandListForQueue(ENUM_QUEUE_TYPE queue_type)
{
	return device->GetCommandBufferManager()->GetOrCreateCommandBuffer(queue_type);
}
// --  
// --  
void VulkanRHI::SubmitCommandListForQueue(CommandList* cmd_list, ENUM_QUEUE_TYPE queue_type)
{
	auto* vk_cmd = STATIC_CAST(cmd_list, VK_CommandBuffer);
	device->GetQueue(queue_type)->Submit(vk_cmd);
	// --   Auto-reset command buffer state after submit so upper layers don't need to
	vk_cmd->command_state = VK_CommandBuffer::EState::NeedReset;
}

void VulkanRHI::SubmitCommandList(CommandList* command_list)
{
	device->GetQueue(ENUM_QUEUE_TYPE::GRAPHICS)->Submit(STATIC_CAST(command_list,VK_CommandBuffer));
}

void VulkanRHI::RenderEnd()
{
	// Check GPU fence completion (non-blocking, async submit mode)
	device->GetQueue(ENUM_QUEUE_TYPE::GRAPHICS)->CheckCompletion(g_frame_number_render_thread.load());
	device->GetMemoryManager()->ReleaseFreedPages();
	device->GetStagingBufferManager()->ProcessPendingFree(false, true);
}

MXRender::RHI::BindlessManager* VulkanRHI::GetBindlessManager()
{
	return device ? device->GetBindlessManager() : nullptr;
}

VK_Device* VulkanRHI::GetDevice() CONST
{
	return device;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
