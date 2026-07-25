#pragma once
#ifndef _VIEWPORT_
#define _VIEWPORT_
#include "Core/ConstDefine.h"
#include "RenderEnum.h"
#include "RenderResource.h"



MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
class UIBase;
MYRENDERER_END_NAMESPACE
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Texture;
class CommandList;
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(Viewport,  public RenderResource)
#pragma region METHOD
public:
	Viewport() MYDEFAULT;
	VIRTUAL ~Viewport() MYDEFAULT;

	//virtual void METHOD(SetViewport)(Float32 x, Float32 y, Float32 width, Float32 height, Float32 min_depth, Float32 max_depth) PURE;
	VIRTUAL Texture* METHOD(GetCurrentBackBufferRTV)() PURE;
	VIRTUAL Texture* METHOD(GetCurrentBackBufferDSV)() PURE;
	VIRTUAL Vector<UInt32> METHOD(GetViewportSize)() CONST PURE;
	VIRTUAL UInt32 METHOD(GetViewportSizeWidth)() CONST PURE;
	VIRTUAL UInt32 METHOD(GetViewportSizeHeight)() CONST PURE;

	VIRTUAL void METHOD(Resize)(UInt32 in_width, UInt32 in_height) PURE;
	VIRTUAL void METHOD(Present)(MXRender::RHI::CommandList* in_cmd_list , bool is_present, bool is_lock_to_vsync) PURE;

	VIRTUAL void METHOD(AttachUiLayer)(UI::UIBase* ui_layer) PURE;
protected:

private:

#pragma endregion


#pragma region MEMBER

#pragma endregion



MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif
