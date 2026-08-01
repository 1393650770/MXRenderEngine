#pragma once
#ifndef _VUKANRENDERRHI_
#define _VUKANRENDERRHI_
#include "RHI/RenderRHI.h"
#include "vulkan/vulkan_core.h"
#include "VK_Viewport.h"
#include <atomic>
#include <thread>


MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Viewport;
class Shader;
class Buffer;

MYRENDERER_BEGIN_NAMESPACE(Vulkan)

class VK_Device;
class VK_Viewport;
class VK_CommandBuffer;
class VK_BindlessManager;


//RHI is mostly responsible for resource creation
//Context is responsible for rendering command commit records
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(VulkanRHI,public RenderRHI)

#pragma region MATHOD

public:
	VulkanRHI() MYDEFAULT;
	VIRTUAL ~VulkanRHI() MYDEFAULT;

#pragma region INIT_MATHOD

	VIRTUAL void METHOD(Init)(RenderFactory* render_factory) OVERRIDE FINAL;


	VIRTUAL void METHOD(PostInit)() OVERRIDE FINAL;


	VIRTUAL void METHOD(Shutdown)() OVERRIDE FINAL;

#pragma endregion


#pragma region CREATE_RESOURCE
	VIRTUAL Viewport* METHOD(CreateViewport)(void* window_handle, Int width, Int height, Bool is_full_screen) OVERRIDE FINAL;
	VIRTUAL Shader* CreateShader(CONST ShaderDesc& desc, CONST ShaderDataPayload& data)  OVERRIDE FINAL;
	VIRTUAL Buffer* METHOD(CreateBuffer)(const BufferDesc& buffer_desc) OVERRIDE FINAL;
	VIRTUAL Texture* METHOD(CreateTexture)(CONST TextureDesc& texture_desc) OVERRIDE FINAL;
	VIRTUAL RenderPipelineState* METHOD(CreateRenderPipelineState)(CONST RenderGraphiPipelineStateDesc& desc) OVERRIDE FINAL;
	// --  
	VIRTUAL ComputePipelineState* METHOD(CreateComputePipelineState)(CONST ComputePipelineStateDesc& desc) OVERRIDE FINAL;
	VIRTUAL RenderPass* METHOD(CreateRenderPass)(CONST RenderPassDesc& desc) OVERRIDE FINAL;
	VIRTUAL FrameBuffer* METHOD(CreateFrameBuffer)(CONST FrameBufferDesc& desc) OVERRIDE FINAL;

	VIRTUAL void* METHOD(MapBuffer)(Buffer* buffer, ENUM_MAP_TYPE map_type, ENUM_MAP_FLAG map_flag) OVERRIDE FINAL;
	VIRTUAL void METHOD(UnmapBuffer)(Buffer* buffer) OVERRIDE FINAL;
#pragma endregion

#pragma region DRAW
	// --   Returns CB for main-thread recording
	VIRTUAL	CommandList* METHOD(GetImmediateCommandList)() OVERRIDE FINAL;
	// --   Async RHI thread: write-CB / swap / rhi-CB for Present
	CommandList* METHOD(GetWriteCommandList)();
	CommandList* METHOD(GetRHICmdListForPresent)();
	void METHOD(SwapCommandLists)();   // atomically swap write <=> rhi, signal RHI thread
	Bool METHOD(IsReplayDone)() CONST; // check if RHI thread finished replay
	VIRTUAL UInt64 METHOD(GetSwapFrame)() CONST OVERRIDE { return swap_frame_.load(std::memory_order_acquire); }
	VIRTUAL UInt64 METHOD(GetReplayFrame)() CONST OVERRIDE { return replay_frame_.load(std::memory_order_acquire); }
	// --
	VIRTUAL CommandList* METHOD(GetCommandListForQueue)(ENUM_QUEUE_TYPE queue_type) OVERRIDE FINAL;
	VIRTUAL void METHOD(SubmitCommandList)(CommandList* command_list) OVERRIDE FINAL;
	// --  
	VIRTUAL void METHOD(SubmitCommandListForQueue)(CommandList* cmd_list, ENUM_QUEUE_TYPE queue_type) OVERRIDE FINAL;
	VIRTUAL void METHOD(RenderEnd)() OVERRIDE FINAL;
	VIRTUAL MXRender::RHI::BindlessManager* METHOD(GetBindlessManager)() OVERRIDE FINAL;
	VK_Device* METHOD(GetDevice)() CONST;
	// --   RHI thread control (lock-free)
	void METHOD(StartRHIThread)();
	void METHOD(StopRHIThread)();
	Bool METHOD(CheckAndProcessReplay)();
#pragma endregion

private:
	Bool METHOD(CheckGpuSuitable)(VkPhysicalDevice gpu);

	VkPhysicalDevice METHOD(GetGpuFromHarddrive)();
	void METHOD(CreateDevice)(Bool enable_validation_layers);
	void METHOD(CreateInstance)(Bool enable_validation_layers, Bool validation_optional);
	void METHOD(InitializeDebugmessenger)(Bool enable_validation_layers);

	Bool METHOD(CheckValidationlayerSupport)();
	Vector<CONST Char*> METHOD(GetRequiredExtensions)(Bool enable_validation_layers);
	void METHOD(PopulateDebugMessengerCreateInfo)(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	VkResult METHOD(CreateDebugUtilsMessengerEXT)(VkInstance instance, CONST VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, CONST VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
protected:

#pragma endregion

#pragma region MEMBER
public:
private:
protected:
	VkInstance instance=VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debug_messenger;
	VK_Device* device=nullptr;
	Vector<VK_Viewport*> viewports;
	VK_CommandBuffer* write_cb=nullptr;    // main thread records into this
	VK_CommandBuffer* rhi_cb=nullptr;     // RHI thread replays this, main thread presents
	VK_CommandBuffer* immediate_command_buffer=nullptr;
	Vector<VK_CommandBuffer*> defered_command_buffers;
	std::atomic<bool> replay_ready{false};
	std::atomic<bool> replay_done{true};
	std::atomic<UInt64> swap_frame_{0};    // incremented per SwapCommandLists
	std::atomic<UInt64> replay_frame_{0};  // frame whose replay the RHI thread finished
	std::atomic<bool> rhi_running{false};
	std::thread rhi_thread;
	friend class VK_Viewport;
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif

