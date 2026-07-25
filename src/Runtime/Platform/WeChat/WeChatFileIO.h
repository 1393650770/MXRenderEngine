#pragma once
#ifndef _WECHAT_FILEIO_
#define _WECHAT_FILEIO_
#if PLATFORM_GLES3

#include "Platform/PlatformFileIO.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Platform)
MYRENDERER_BEGIN_NAMESPACE(WeChat)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(WeChatFileIO, public PlatformFileIO)
#pragma region METHOD
public:
	WeChatFileIO() MYDEFAULT;
	// Phase 3+: wx.getFileSystemManager bridge via EM_ASM
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif
#endif
