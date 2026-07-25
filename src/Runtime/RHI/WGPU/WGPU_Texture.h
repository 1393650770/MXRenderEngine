#pragma once
#ifndef _WGPU_TEXTURE_
#define _WGPU_TEXTURE_

#if PLATFORM_WGPU

#include "RHI/RenderTexture.h"
#include <webgpu/webgpu.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(WGPU_Texture, public Texture)
#pragma region METHOD
public:
	WGPU_Texture(CONST TextureDesc& desc);
	VIRTUAL ~WGPU_Texture() OVERRIDE;

	// For swapchain backbuffer: wraps an existing WGPUTextureView (no ownership)
	void METHOD(SetSwapChainTexture)(WGPUTexture texture, WGPUTextureView view);
	// For owned textures: created via wgpuDeviceCreateTexture
	void METHOD(SetOwnedTexture)(WGPUTexture texture, WGPUTextureView view);

	WGPUTextureView METHOD(GetView)() CONST { return m_view; }
	WGPUTexture METHOD(GetTexture)() CONST { return m_texture; }
	Bool METHOD(IsSwapChain)() CONST { return m_is_swapchain; }
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	WGPUTexture m_texture = nullptr;
	WGPUTextureView m_view = nullptr;
	Bool m_is_swapchain = false;  // swapchain textures are not released by us
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
#endif // _WGPU_TEXTURE_
