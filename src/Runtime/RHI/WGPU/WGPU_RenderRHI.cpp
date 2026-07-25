#if PLATFORM_WGPU

#include "RHI/WGPU/WGPU_RenderRHI.h"
#include "RHI/WGPU/WGPU_CommandBuffer.h"
#include "RHI/WGPU/WGPU_Viewport.h"
#include "RHI/WGPU/WGPU_Texture.h"
#include "RHI/WGPU/WGPU_Shader.h"
#include "RHI/WGPU/WGPU_PipelineState.h"
#include "RHI/WGPU/WGPU_Buffer.h"
#include "RHI/RenderEnum.h"
#include <iostream>
#include <string.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

// ---- Async init callbacks ----
// Browser WebGPU adapter/device requests are callback-based; we chain:
//   Init() → requestAdapter → OnAdapterRequestEnded → requestDevice → OnDeviceRequestEnded
// The ready flag flips only after device+queue are available.

static void OnDeviceRequestEnded(WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* userdata)
{
	auto* rhi = STATIC_CAST(userdata, WGPU_RenderRHI);
	if (status == WGPURequestDeviceStatus_Success)
	{
		rhi->m_device = device;
		rhi->m_queue = wgpuDeviceGetQueue(device);
		rhi->m_is_ready = true;
		std::cout << "[WGPU] Device ready." << std::endl;
	}
	else
	{
		std::cout << "[WGPU] Device request failed: " << (message ? message : "unknown") << std::endl;
	}
}

static void OnAdapterRequestEnded(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message, void* userdata)
{
	auto* rhi = STATIC_CAST(userdata, WGPU_RenderRHI);
	if (status == WGPURequestAdapterStatus_Success)
	{
		rhi->m_adapter = adapter;
		// Chain: now request the device (async callback)
		wgpuAdapterRequestDevice(adapter, nullptr, OnDeviceRequestEnded, rhi);
		std::cout << "[WGPU] Adapter ready, requesting device..." << std::endl;
	}
	else
	{
		std::cout << "[WGPU] Adapter request failed: " << (message ? message : "unknown") << std::endl;
	}
}

#pragma region INIT_METHOD

void WGPU_RenderRHI::Init(RenderFactory* render_factory)
{
	// Create instance
	WGPUInstanceDescriptor inst_desc{};
	m_instance = wgpuCreateInstance(&inst_desc);
	CHECK_WITH_LOG(m_instance == nullptr, "WGPU: failed to create instance");

	// Kick off async adapter request (callback resolves on the browser event loop)
	wgpuInstanceRequestAdapter(m_instance, nullptr, OnAdapterRequestEnded, this);

	std::cout << "[WGPU] Init: adapter request dispatched." << std::endl;
}

void WGPU_RenderRHI::PostInit()
{
	// No-op for Single-threaded WebGPU backend
}

void WGPU_RenderRHI::Shutdown()
{
	// Release PSO storage (Decision #8: RHI owns all PSOs)
	for (auto* pso : m_pso_storage)
	{
		if (pso) delete pso;
	}
	m_pso_storage.clear();

	if (m_immediate_cmd) { delete m_immediate_cmd; m_immediate_cmd = nullptr; }
	// Viewport + surface are owned by the caller (EmscriptenWindow), not the RHI.
	// SwapChain is owned by the viewport. Do not release them here.
	if (m_queue) { wgpuQueueRelease(m_queue); m_queue = nullptr; }
	if (m_device) { wgpuDeviceRelease(m_device); m_device = nullptr; }
	if (m_adapter) { wgpuAdapterRelease(m_adapter); m_adapter = nullptr; }
	if (m_instance) { wgpuInstanceRelease(m_instance); m_instance = nullptr; }
	m_is_ready = false;
}

#pragma endregion

#pragma region HELPERS

// Map engine texture format to WebGPU format. Only formats used by
// HelloTriangle + swapchain are handled; expand as needed.
static WGPUTextureFormat TranslateFormat(ENUM_TEXTURE_FORMAT fmt)
{
	switch (fmt)
	{
	case ENUM_TEXTURE_FORMAT::BGRA8:   return WGPUTextureFormat_BGRA8Unorm;
	case ENUM_TEXTURE_FORMAT::RGBA8:   return WGPUTextureFormat_RGBA8Unorm;
	case ENUM_TEXTURE_FORMAT::RGBA16F: return WGPUTextureFormat_RGBA16Float;
	case ENUM_TEXTURE_FORMAT::R32U:    return WGPUTextureFormat_R32Uint;
	case ENUM_TEXTURE_FORMAT::D32:     return WGPUTextureFormat_Depth32Float;
	case ENUM_TEXTURE_FORMAT::D24S8:   return WGPUTextureFormat_Depth24PlusStencil8;
	default:                            return WGPUTextureFormat_Undefined;
	}
}

static WGPUPrimitiveTopology TranslateTopology(ENUM_PRIMITIVE_TYPE topo)
{
	switch (topo)
	{
	case ENUM_PRIMITIVE_TYPE::TriangleList:  return WGPUPrimitiveTopology_TriangleList;
	case ENUM_PRIMITIVE_TYPE::TriangleStrip: return WGPUPrimitiveTopology_TriangleStrip;
	case ENUM_PRIMITIVE_TYPE::LineList:      return WGPUPrimitiveTopology_LineList;
	case ENUM_PRIMITIVE_TYPE::PointList:     return WGPUPrimitiveTopology_PointList;
	default:                                  return WGPUPrimitiveTopology_TriangleList;
	}
}

// WGSL entry point name by stage (tint-converted GLSL uses vs_main/fs_main)
static const char* GetWgslEntryPoint(ENUM_SHADER_STAGE stage)
{
	switch (stage)
	{
	case ENUM_SHADER_STAGE::Shader_Vertex:  return "vs_main";
	case ENUM_SHADER_STAGE::Shader_Pixel:   return "fs_main";
	case ENUM_SHADER_STAGE::Shader_Compute: return "cs_main";
	default:                                 return "main";
	}
}

#pragma endregion

#pragma region CREATE_RESOURCE

Viewport* WGPU_RenderRHI::CreateViewport(void* window_handle, Int width, Int height, Bool is_full_screen)
{
	CHECK_WITH_LOG(m_device == nullptr, "WGPU: CreateViewport called before device ready");
	CHECK_WITH_LOG(m_instance == nullptr, "WGPU: CreateViewport called with null instance");

	// window_handle is the canvas selector string (from EmscriptenWindow::GetNativeHandle)
	const char* canvas_selector = "#canvas";
	if (window_handle) canvas_selector = static_cast<const char*>(window_handle);

	// Create surface from canvas HTML selector
	WGPUSurfaceDescriptorFromCanvasHTMLSelector canvas_desc{};
	canvas_desc.chain.next = nullptr;
	canvas_desc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
	canvas_desc.selector = canvas_selector;

	WGPUSurfaceDescriptor surf_desc{};
	surf_desc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&canvas_desc);

	// Surface ownership: passed to WGPU_Viewport (released in its destructor).
	// RHI does NOT store/release it — matches VK backend pattern where the
	// caller (Window/EmscriptenWindow) owns the returned viewport.
	WGPUSurface surface = wgpuInstanceCreateSurface(m_instance, &surf_desc);
	CHECK_WITH_LOG(surface == nullptr, "WGPU: wgpuInstanceCreateSurface failed");

	m_swapchain_width = (UInt32)width;
	m_swapchain_height = (UInt32)height;

	return new WGPU_Viewport(m_device, surface, (UInt32)width, (UInt32)height);
}

Shader* WGPU_RenderRHI::CreateShader(CONST ShaderDesc& desc, CONST ShaderDataPayload& data)
{
	CHECK_WITH_LOG(m_device == nullptr, "WGPU: CreateShader called before device ready");
	CHECK_WITH_LOG(data.wgsl_source.empty(), "WGPU: CreateShader called with empty WGSL source");

	// Create shader module from WGSL source
	WGPUShaderModuleWGSLDescriptor wgsl_desc{};
	wgsl_desc.chain.next = nullptr;
	wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
	wgsl_desc.code = data.wgsl_source.c_str();

	WGPUShaderModuleDescriptor module_desc{};
	module_desc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&wgsl_desc);

	WGPUShaderModule module = wgpuDeviceCreateShaderModule(m_device, &module_desc);
	CHECK_WITH_LOG(module == nullptr, "WGPU: wgpuDeviceCreateShaderModule failed");

	auto* shader = new WGPU_Shader(desc, data);
	shader->SetModule(module);
	return shader;
}

Buffer* WGPU_RenderRHI::CreateBuffer(CONST BufferDesc& buffer_desc)
{
	CHECK_WITH_LOG(m_device == nullptr, "WGPU: CreateBuffer called before device ready");
	// Minimal: HelloTriangle doesn't use buffers (hardcoded triangle in shader).
	// Full implementation: WGPUBufferDescriptor + wgpuDeviceCreateBuffer + SetBuffer.
	auto* buf = new WGPU_Buffer(buffer_desc);
	return buf;
}

Texture* WGPU_RenderRHI::CreateTexture(CONST TextureDesc& texture_desc)
{
	CHECK_WITH_LOG(m_device == nullptr, "WGPU: CreateTexture called before device ready");
	// Minimal: HelloTriangle uses swapchain textures only (created by WGPU_Viewport).
	auto* tex = new WGPU_Texture(texture_desc);
	return tex;
}

RenderPipelineState* WGPU_RenderRHI::CreateRenderPipelineState(CONST RenderGraphiPipelineStateDesc& desc)
{
	CHECK_WITH_LOG(m_device == nullptr, "WGPU: CreateRenderPipelineState called before device ready");

	// Get vertex shader module
	WGPUShaderModule vs_module = nullptr;
	if (desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex])
	{
		auto* vs = static_cast<WGPU_Shader*>(desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex]);
		vs_module = vs->GetModule();
	}
	CHECK_WITH_LOG(vs_module == nullptr, "WGPU: PSO missing vertex shader module");

	// Get pixel shader module
	WGPUShaderModule ps_module = nullptr;
	if (desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel])
	{
		auto* ps = static_cast<WGPU_Shader*>(desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel]);
		ps_module = ps->GetModule();
	}

	// Determine render target format from desc.render_targets or default to swapchain format
	WGPUTextureFormat rt_format = WGPUTextureFormat_BGRA8Unorm;  // matches WGPU_Viewport swapchain
	if (!desc.render_targets.empty() && desc.render_targets[0])
	{
		auto* rt_tex = static_cast<WGPU_Texture*>(desc.render_targets[0]);
		(void)rt_tex;  // format derived from TextureDesc in a fuller impl; swapchain default is fine for HelloTriangle
	}

	// Build vertex state (no vertex buffers: triangle hardcoded via vertex_index)
	WGPUVertexState vertex_state{};
	vertex_state.module = vs_module;
	vertex_state.entryPoint = GetWgslEntryPoint(ENUM_SHADER_STAGE::Shader_Vertex);
	vertex_state.bufferCount = 0;
	vertex_state.buffers = nullptr;

	// Build fragment state (optional: HelloTriangle has one)
	WGPUFragmentState fragment_state{};
	WGPUColorTargetState color_target{};
	color_target.format = rt_format;
	color_target.blend = nullptr;  // no blending for HelloTriangle
	color_target.writeMask = WGPUColorWriteMask_All;

	if (ps_module)
	{
		fragment_state.module = ps_module;
		fragment_state.entryPoint = GetWgslEntryPoint(ENUM_SHADER_STAGE::Shader_Pixel);
		fragment_state.targetCount = 1;
		fragment_state.targets = &color_target;
	}

	// Build pipeline descriptor
	WGPURenderPipelineDescriptor pipeline_desc{};
	pipeline_desc.label = "WGPU_PipelineState";
	pipeline_desc.layout = nullptr;  // auto-layout from shader reflection
	pipeline_desc.vertex = vertex_state;
	pipeline_desc.fragment = ps_module ? &fragment_state : nullptr;
	pipeline_desc.primitive.topology = TranslateTopology(desc.primitive_topology);
	pipeline_desc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
	pipeline_desc.primitive.frontFace = WGPUFrontFace_CCW;
	pipeline_desc.primitive.cullMode = WGPUCullMode_None;

	// No multisampling for HelloTriangle
	pipeline_desc.multisample.count = 1;
	pipeline_desc.multisample.mask = 0xFFFFFFFF;
	pipeline_desc.multisample.alphaToCoverageEnabled = false;

	// No depth/stencil for HelloTriangle (DSV is null on WebGPU viewport)
	pipeline_desc.depthStencil = nullptr;

	WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(m_device, &pipeline_desc);
	CHECK_WITH_LOG(pipeline == nullptr, "WGPU: wgpuDeviceCreateRenderPipeline failed");

	auto* pso = new WGPU_PipelineState(desc);
	pso->SetPipeline(pipeline);

	// Take ownership (Decision #8): store in m_pso_storage, external code must not delete
	m_pso_storage.push_back(pso);
	return pso;
}

ComputePipelineState* WGPU_RenderRHI::CreateComputePipelineState(CONST ComputePipelineStateDesc& desc)
{
	// Phase C stub: HelloTriangle doesn't use compute
	std::cout << "[WGPU] CreateComputePipelineState: stub (not used by HelloTriangle)" << std::endl;
	return nullptr;
}

RenderPass* WGPU_RenderRHI::CreateRenderPass(CONST RenderPassDesc& desc)
{
	// WebGPU has no persistent RenderPass object (beginRenderPass is inline)
	return nullptr;
}

FrameBuffer* WGPU_RenderRHI::CreateFrameBuffer(CONST FrameBufferDesc& desc)
{
	// WebGPU has no persistent FrameBuffer object
	return nullptr;
}

void* WGPU_RenderRHI::MapBuffer(Buffer* buffer, ENUM_MAP_TYPE map_type, ENUM_MAP_FLAG map_flag)
{
	// Phase C minimal: HelloTriangle doesn't map buffers.
	// Full implementation: wgpuQueueWriteBuffer for uploads (Decision #13).
	return nullptr;
}

void WGPU_RenderRHI::UnmapBuffer(Buffer* buffer)
{
	// Phase C minimal: no-op
}

#pragma endregion

#pragma region DRAW

CommandList* WGPU_RenderRHI::GetImmediateCommandList()
{
	if (!m_immediate_cmd && m_device)
	{
		m_immediate_cmd = new WGPU_CommandBuffer(m_device);
	}
	// Match VK backend pattern: Begin() on each access (idempotent if already begun)
	if (m_immediate_cmd)
	{
		m_immediate_cmd->Begin();
	}
	return m_immediate_cmd;
}

CommandList* WGPU_RenderRHI::GetCommandListForQueue(ENUM_QUEUE_TYPE queue_type)
{
	return GetImmediateCommandList();
}

void WGPU_RenderRHI::SubmitCommandList(CommandList* command_list)
{
	if (!command_list || !m_queue) return;

	auto* wgpu_cmd = static_cast<WGPU_CommandBuffer*>(command_list);
	WGPUCommandBuffer encoded = wgpu_cmd->GetEncodedCommandBuffer();
	if (!encoded) return;

	wgpuQueueSubmit(m_queue, 1, &encoded);
	wgpu_cmd->ConsumeEncodedCommandBuffer();
}

void WGPU_RenderRHI::SubmitCommandListForQueue(CommandList* cmd_list, ENUM_QUEUE_TYPE queue_type)
{
	SubmitCommandList(cmd_list);
}

void WGPU_RenderRHI::RenderEnd()
{
	// No-op for Single-threaded mode
}

CommandList* WGPU_RenderRHI::GetWriteCommandList()
{
	return GetImmediateCommandList();
}

CommandList* WGPU_RenderRHI::GetRHICmdListForPresent()
{
	return GetImmediateCommandList();
}

void WGPU_RenderRHI::SwapCommandLists()
{
	// No-op for Single-threaded mode
}

Bool WGPU_RenderRHI::IsReplayDone() CONST
{
	return true;  // Single-threaded: replay is a no-op
}

void WGPU_RenderRHI::StartRHIThread()
{
	// No-op: WebGPU backend is Single-threaded only
}

void WGPU_RenderRHI::StopRHIThread()
{
	// No-op: WebGPU backend is Single-threaded only
}

BindlessManager* WGPU_RenderRHI::GetBindlessManager()
{
	return nullptr;  // Bindless skipped per plan Decision #3
}

#pragma endregion

void WGPU_RenderRHI::ConfigureSwapChain(UInt32 width, UInt32 height)
{
	m_swapchain_width = width;
	m_swapchain_height = height;
	if (m_viewport)
	{
		m_viewport->Resize(width, height);
	}
}

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
