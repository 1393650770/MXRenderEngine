#pragma once
#ifndef _AUDIO_SYSTEM_
#define _AUDIO_SYSTEM_

// Audio System Abstraction
// Patterned after UISystem: virtual methods with empty defaults.
// Platform-specific implementations in Platform/<Platform>/

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)

MYRENDERER_BEGIN_CLASS(AudioSystem)
#pragma region METHOD
public:
	AudioSystem() MYDEFAULT;
	VIRTUAL ~AudioSystem() MYDEFAULT;

	VIRTUAL void METHOD(PlayBGM)(CONST String& path, Bool loop = true) {}
	VIRTUAL void METHOD(StopBGM)() {}
	VIRTUAL void METHOD(PlaySFX)(CONST String& path) {}
	VIRTUAL void METHOD(SetVolume)(Float32 vol) {}

protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _AUDIO_SYSTEM_
