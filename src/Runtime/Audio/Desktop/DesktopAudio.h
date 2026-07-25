#pragma once
#ifndef _DESKTOP_AUDIO_
#define _DESKTOP_AUDIO_

// Desktop Audio Stub — platform-specific implementation goes here (e.g. OpenAL)

#include "Audio/AudioSystem.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)
MYRENDERER_BEGIN_NAMESPACE(Desktop)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(DesktopAudio, public AudioSystem)
#pragma region METHOD
public:
	DesktopAudio() MYDEFAULT;
	// Override playback methods when implementation is ready
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Desktop
MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender

#endif
