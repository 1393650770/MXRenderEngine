#if PLATFORM_WGPU

#include "RHI/WGPU/WGPU_Texture.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

WGPU_Texture::WGPU_Texture(CONST TextureDesc& desc)
	: Texture(desc)
{
}

WGPU_Texture::~WGPU_Texture()
{
	// Swapchain textures are owned by the swapchain (don't release WGPUTexture),
	// but the WGPUTextureView acquired via wgpuSwapChainGetCurrentTextureView IS
	// application-owned and must be released in both cases.
	if (m_view) { wgpuTextureViewRelease(m_view); m_view = nullptr; }
	if (!m_is_swapchain)
	{
		if (m_texture) { wgpuTextureRelease(m_texture); m_texture = nullptr; }
	}
}

void WGPU_Texture::SetSwapChainTexture(WGPUTexture texture, WGPUTextureView view)
{
	// Release previous view (application-owned); texture is swapchain-owned.
	// Without this, each per-frame re-acquire leaks the previous WGPUTextureView.
	if (m_view) { wgpuTextureViewRelease(m_view); }
	m_texture = texture;
	m_view = view;
	m_is_swapchain = true;
}

void WGPU_Texture::SetOwnedTexture(WGPUTexture texture, WGPUTextureView view)
{
	m_texture = texture;
	m_view = view;
	m_is_swapchain = false;
}

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
