#include "PreviewViewport.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

void PreviewViewport::SetTargets(RHI::Texture* color, RHI::Texture* depth, UInt32 width, UInt32 height)
{
	m_color = color;
	m_depth = depth;
	m_width = width;
	m_height = height;
}

void PreviewViewport::Resize(UInt32 in_width, UInt32 in_height)
{
	// The panel re-creates textures on resize; store the intent only.
	m_width = in_width;
	m_height = in_height;
}

MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
