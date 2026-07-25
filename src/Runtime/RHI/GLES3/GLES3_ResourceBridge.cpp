#if PLATFORM_GLES3

// GLES3 stubs for Render-layer resource pooling bridge functions.
// These functions are declared in RenderGraphResource.h and implemented in
// VK_ResourcePool.cpp (Vulkan backend). For GLES3 Phase 0, we provide no-op
// stubs: CreateTexture/CreateBuffer directly (no pooling).
// Phase 2+ can add a GLES3 resource pool if profiling shows allocation overhead.

#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderTexture.h"
#include "RHI/RenderBuffer.h"
#include "Core/ConstDefine.h"
#include <memory>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)

std::unique_ptr<MXRender::RHI::Texture> AcquirePooledTexture(CONST MXRender::RHI::TextureDesc& desc)
{
	// Phase 0: no pooling — create a new texture each time.
	// The caller (RenderGraph Resource::Realize) assumes ownership.
	MXRender::RHI::Texture* tex = ::g_render_rhi->CreateTexture(desc);
	return std::unique_ptr<MXRender::RHI::Texture>(tex);
}

void ReturnPooledTexture(std::unique_ptr<MXRender::RHI::Texture> texture, CONST MXRender::RHI::TextureDesc& desc)
{
	// Phase 0: no pooling — just destroy.
	// The unique_ptr destructor calls ~Texture which cleans up GL resources.
	(void)desc;
	// texture goes out of scope and is deleted
}

std::unique_ptr<MXRender::RHI::Buffer> AcquirePooledBuffer(CONST MXRender::RHI::BufferDesc& desc)
{
	// Phase 0: no pooling
	extern MXRender::RHI::RenderRHI* g_render_rhi;
	MXRender::RHI::Buffer* buf = g_render_rhi->CreateBuffer(desc);
	return std::unique_ptr<MXRender::RHI::Buffer>(buf);
}

void ReturnPooledBuffer(std::unique_ptr<MXRender::RHI::Buffer> buffer, CONST MXRender::RHI::BufferDesc& desc)
{
	(void)desc;
	// unique_ptr destructor cleans up
}

void SetDebugNameForRHIResource(MXRender::RHI::RenderResource* resource, CONST String& name)
{
	// Phase 0: GLES3 has no debug name support.
	(void)resource;
	(void)name;
}

MYRENDERER_END_NAMESPACE  // Render
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
