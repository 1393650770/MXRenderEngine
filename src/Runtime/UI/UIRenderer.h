
#pragma once
#ifndef _UIRENDERER_
#define _UIRENDERER_

#include "Core/ConstDefine.h"

// Forward-declare RHI types (no Vulkan dependency)
MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class CommandList;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

/**
 * Abstract UI rendering backend — slimmed to the public API surface.
 *
 * The 12 RmlUi-specific methods (CompileGeometry, DrawGeometry, SetScissor,
 * EnableClipMask, etc.) have been moved to RmlUIRenderer's own public
 * (non-virtual) methods.  Only the generic BeginFrame/EndFrame pair plus
 * query methods remain on the abstract interface.
 */
MYRENDERER_BEGIN_CLASS(UIRenderer)

#pragma region METHOD
public:
	VIRTUAL ~UIRenderer() MYDEFAULT;

	/// Called once per frame before any draw calls.
	VIRTUAL void METHOD(BeginFrame)(RHI::CommandList* cmd) { (void)cmd; }

	/// Called once per frame after all draw calls.
	VIRTUAL void METHOD(EndFrame)(RHI::CommandList* cmd) { (void)cmd; }

	/// Returns true if offscreen rendering is required this frame.
	VIRTUAL bool METHOD(NeedsOffscreen)() CONST { return false; }

	/// Returns the current viewport height used for coordinate flipping.
	VIRTUAL UInt32 METHOD(GetViewportHeight)() CONST { return 0; }

protected:
private:
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // !_UIRENDERER_
