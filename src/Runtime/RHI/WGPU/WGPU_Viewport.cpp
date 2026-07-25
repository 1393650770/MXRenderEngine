#if PLATFORM_WGPU

#include "RHI/WGPU/WGPU_Viewport.h"
#include "RHI/WGPU/WGPU_Texture.h"
#include "RHI/WGPU/WGPU_CommandBuffer.h"
#include "RHI/RenderCommandList.h"
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

WGPU_Viewport::WGPU_Viewport(WGPUDevice device, WGPUSurface surface, UInt32 width, UInt32 height)
	: m_device(device), m_surface(surface), m_width(width), m_height(height)
{
	// Create swapchain
	WGPUSwapChainDescriptor sc_desc{};
	sc_desc.usage = WGPUTextureUsage_RenderAttachment;
	sc_desc.format = WGPUTextureFormat_BGRA8Unorm;
	sc_desc.width = width;
	sc_desc.height = height;
	sc_desc.presentMode = WGPUPresentMode_Fifo;
	m_swap_chain = wgpuDeviceCreateSwapChain(m_device, m_surface, &sc_desc);

	// Create the backbuffer texture wrapper (view acquired per-frame)
	TextureDesc tex_desc;
	tex_desc.width = width;
	tex_desc.height = height;
	tex_desc.format = ENUM_TEXTURE_FORMAT::BGRA8;
	tex_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	tex_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_RENDER_TARGET;
	m_backbuffer_tex = new WGPU_Texture(tex_desc);

	std::cout << "[WGPU] Viewport created: " << width << "x" << height << std::endl;
}

WGPU_Viewport::~WGPU_Viewport()
{
	if (m_backbuffer_tex) { delete m_backbuffer_tex; m_backbuffer_tex = nullptr; }
	if (m_swap_chain) { wgpuSwapChainRelease(m_swap_chain); m_swap_chain = nullptr; }
	if (m_surface) { wgpuSurfaceRelease(m_surface); m_surface = nullptr; }
}

Texture* WGPU_Viewport::GetCurrentBackBufferRTV()
{
	if (!m_swap_chain) return nullptr;
	// Acquire the current swapchain texture view and wrap it
	WGPUTextureView view = wgpuSwapChainGetCurrentTextureView(m_swap_chain);
	if (!view) return nullptr;
	WGPUTexture tex = wgpuSwapChainGetCurrentTexture(m_swap_chain);
	m_backbuffer_tex->SetSwapChainTexture(tex, view);
	return m_backbuffer_tex;
}

Texture* WGPU_Viewport::GetCurrentBackBufferDSV()
{
	// HelloTriangle: no depth buffer (PSO created without depth attachment)
	return nullptr;
}

Vector<UInt32> WGPU_Viewport::GetViewportSize() CONST
{
	return { m_width, m_height };
}

UInt32 WGPU_Viewport::GetViewportSizeWidth() CONST
{
	return m_width;
}

UInt32 WGPU_Viewport::GetViewportSizeHeight() CONST
{
	return m_height;
}

void WGPU_Viewport::Resize(UInt32 in_width, UInt32 in_height)
{
	if (m_width == in_width && m_height == in_height) return;
	m_width = in_width;
	m_height = in_height;

	// Recreate swapchain
	if (m_swap_chain) { wgpuSwapChainRelease(m_swap_chain); m_swap_chain = nullptr; }
	WGPUSwapChainDescriptor sc_desc{};
	sc_desc.usage = WGPUTextureUsage_RenderAttachment;
	sc_desc.format = WGPUTextureFormat_BGRA8Unorm;
	sc_desc.width = in_width;
	sc_desc.height = in_height;
	sc_desc.presentMode = WGPUPresentMode_Fifo;
	m_swap_chain = wgpuDeviceCreateSwapChain(m_device, m_surface, &sc_desc);
}

void WGPU_Viewport::Present(CommandList* in_cmd_list, bool is_present, bool is_lock_to_vsync)
{
	if (!m_swap_chain) return;

	// End command recording + submit before presenting
	if (in_cmd_list && m_device)
	{
		auto* wgpu_cmd = static_cast<WGPU_CommandBuffer*>(in_cmd_list);
		// End() finishes the command encoder → produces WGPUCommandBuffer
		wgpu_cmd->End();

		WGPUCommandBuffer encoded = wgpu_cmd->GetEncodedCommandBuffer();
		if (encoded)
		{
			WGPUQueue queue = wgpuDeviceGetQueue(m_device);
			wgpuQueueSubmit(queue, 1, &encoded);
			wgpu_cmd->ConsumeEncodedCommandBuffer();
		}
	}

	if (is_present)
	{
		wgpuSwapChainPresent(m_swap_chain);
	}
}

WGPUTextureView WGPU_Viewport::AcquireCurrentTextureView()
{
	if (!m_swap_chain) return nullptr;
	return wgpuSwapChainGetCurrentTextureView(m_swap_chain);
}

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
