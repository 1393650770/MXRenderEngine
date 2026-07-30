#pragma once
#ifndef _DESKTOP_AUDIO_
#define _DESKTOP_AUDIO_

#include "Audio/Desktop/MiniAudioEngine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)
MYRENDERER_BEGIN_NAMESPACE(Desktop)

// Backward-compatible name for the default desktop audio backend.
using DesktopAudio = MiniAudioEngine;

MYRENDERER_END_NAMESPACE  // Desktop
MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _DESKTOP_AUDIO_
