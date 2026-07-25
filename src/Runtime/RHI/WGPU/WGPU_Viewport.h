#pragma once
#ifndef _WGPU_VIEWPORT_
#define _WGPU_VIEWPORT_

#if PLATFORM_WGPU

#include "RHI/RenderViewport.h"
#include <webgpu/webgpu.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

class WGPU_Texture;

// WGPU_Viewport: owns the WebGPU surface + swapchain.
// Created by WGPU_RenderRHI::CreateViewport once the async device init completes.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(WGPU_Viewport, public Viewport)
#pragma region METHOD
public:
	WGPU_Viewport(WGPUDevice device, WGPUSurface surface, UInt32 width, UInt32 height);
	VIRTUAL ~WGPU_Viewport() OVERRIDE;

	VIRTUAL Texture* METHOD(GetCurrentBackBufferRTV)() OVERRIDE FINAL;
	VIRTUAL Texture* METHOD(GetCurrentBackBufferDSV)() OVERRIDE FINAL;
	VIRTUAL Vector<UInt32> METHOD(GetViewportSize)() CONST OVERRIDE FINAL;
	VIRTUAL UInt32 METHOD(GetViewportSizeWidth)() CONST OVERRIDE FINAL;
	VIRTUAL UInt32 METHOD(GetViewportSizeHeight)() CONST OVERRIDE FINAL;
	VIRTUAL void METHOD(Resize)(UInt32 in_width, UInt32 in_height) OVERRIDE FINAL;
	VIRTUAL void METHOD(Present)(CommandList* in_cmd_list, bool is_present, bool is_lock_to_vsync) OVERRIDE FINAL;
	VIRTUAL void METHOD(AttachUiLayer)(UI::UIBase* ui_layer) OVERRIDE FINAL {}

	WGPUTextureView METHOD(AcquireCurrentTextureView)();
	WGPUSwapChain METHOD(GetSwapChain)() CONST { return m_swap_chain; }
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	WGPUDevice m_device = nullptr;
	WGPUSurface m_surface = nullptr;
	WGPUSwapChain m_swap_chain = nullptr;
	UInt32 m_width = 0;
	UInt32 m_height = 0;

	WGPU_Texture* m_backbuffer_tex = nullptr;  // wraps the current swapchain texture view
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
#endif // _WGPU_VIEWPORT_
