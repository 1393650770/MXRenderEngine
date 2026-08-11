#pragma once
#ifndef _PREVIEW_VIEWPORT_
#define _PREVIEW_VIEWPORT_

#include "RHI/RenderViewport.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

/// RHI::Viewport adapter that points at the UI preview panel's own offscreen
/// textures instead of a swapchain. Lets RmlUISystem::Init (which requires a
/// RHI::Viewport*) run unchanged inside the Editor. Pure RHI-abstract — no
/// Vulkan, no RmlUi.
class PreviewViewport : public RHI::Viewport
{
public:
	PreviewViewport() = default;
	~PreviewViewport() override = default;

	void SetTargets(RHI::Texture* color, RHI::Texture* depth, UInt32 width, UInt32 height);

	RHI::Texture* GetCurrentBackBufferRTV() override { return m_color; }
	RHI::Texture* GetCurrentBackBufferDSV() override { return m_depth; }
	Vector<UInt32> GetViewportSize() const override { return { m_width, m_height }; }
	UInt32 GetViewportSizeWidth() const override { return m_width; }
	UInt32 GetViewportSizeHeight() const override { return m_height; }
	void Resize(UInt32 in_width, UInt32 in_height) override;   // applied next SetTargets
	void Present(RHI::CommandList*, bool, bool) override {}    // no swapchain — no-op
	void AttachUiLayer(UI::UIBase*) override {}                // ImGui lives elsewhere

private:
	RHI::Texture* m_color = nullptr;
	RHI::Texture* m_depth = nullptr;
	UInt32 m_width = 640;
	UInt32 m_height = 480;
};

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_PREVIEW_VIEWPORT_
