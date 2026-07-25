#if PLATFORM_GLES3

#include "RHI/GLES3/GLES3_Viewport.h"
#include "RHI/GLES3/GLES3_Texture.h"
#include "RHI/GLES3/GLES3_CommandBuffer.h"
#include "RHI/RenderRHI.h"
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

GLES3_Viewport::GLES3_Viewport(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx, UInt32 width, UInt32 height)
	: m_context(ctx), m_width(width), m_height(height)
{
	// Create backbuffer texture wrapper (default framebuffer 0)
	TextureDesc tex_desc;
	tex_desc.width = width;
	tex_desc.height = height;
	tex_desc.format = ENUM_TEXTURE_FORMAT::BGRA8;
	tex_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	tex_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_COLOR_ATTACHMENT;
	tex_desc.resource_state = ENUM_RESOURCE_STATE::RenderTarget;

	m_backbuffer_tex = new GLES3_Texture(tex_desc);
	m_backbuffer_tex->SetAsDefaultFramebuffer();

	TextureDesc ds_desc;
	ds_desc.width = width; ds_desc.height = height;
	ds_desc.format = ENUM_TEXTURE_FORMAT::D24S8;
	ds_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_DEPTH;
	m_depth_tex = new GLES3_Texture(ds_desc);
	m_depth_tex->SetAsDefaultFramebuffer();

	// Set initial viewport
	glViewport(0, 0, (GLsizei)width, (GLsizei)height);
	glScissor(0, 0, (GLsizei)width, (GLsizei)height);

	std::cout << "[GLES3] Viewport created: " << width << "x" << height << std::endl;
}

GLES3_Viewport::~GLES3_Viewport()
{
	if (m_backbuffer_tex) { delete m_backbuffer_tex; m_backbuffer_tex = nullptr; }
	if (m_depth_tex) { delete m_depth_tex; m_depth_tex = nullptr; }
}

Texture* GLES3_Viewport::GetCurrentBackBufferRTV()
{
	return m_backbuffer_tex;
}

Texture* GLES3_Viewport::GetCurrentBackBufferDSV()
{
	// Default FBO has implicit depth/stencil from WebGL context creation attrs
	return m_depth_tex;
}

Vector<UInt32> GLES3_Viewport::GetViewportSize() CONST
{
	return { m_width, m_height };
}

UInt32 GLES3_Viewport::GetViewportSizeWidth() CONST
{
	return m_width;
}

UInt32 GLES3_Viewport::GetViewportSizeHeight() CONST
{
	return m_height;
}

void GLES3_Viewport::Resize(UInt32 in_width, UInt32 in_height)
{
	if (m_width == in_width && m_height == in_height) return;
	m_width = in_width;
	m_height = in_height;
	glViewport(0, 0, (GLsizei)m_width, (GLsizei)m_height);
	std::cout << "[GLES3] Viewport resized: " << in_width << "x" << in_height << std::endl;
}

void GLES3_Viewport::Present(CommandList* in_cmd_list, bool is_present, bool is_lock_to_vsync)
{
	if (in_cmd_list)
	{
		in_cmd_list->End();
	}
	// GLES3 on Emscripten: browser swaps the canvas automatically at the end
	// of each requestAnimationFrame callback. No explicit swap needed.
	(void)is_present;
	(void)is_lock_to_vsync;
}

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
