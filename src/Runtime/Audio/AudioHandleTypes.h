#pragma once
#ifndef _AUDIO_HANDLE_TYPES_
#define _AUDIO_HANDLE_TYPES_

#include "Core/ResourceHandle.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)

struct TagAudioClip {};
using AudioClipHandle = ResourceHandle<TagAudioClip>;

MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _AUDIO_HANDLE_TYPES_
