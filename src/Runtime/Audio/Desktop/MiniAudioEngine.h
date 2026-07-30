#pragma once
#ifndef _MINI_AUDIO_ENGINE_
#define _MINI_AUDIO_ENGINE_

#include "Audio/AudioSystem.h"
#include <memory>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)
MYRENDERER_BEGIN_NAMESPACE(Desktop)

// Desktop AudioSystem strategy backed by miniaudio.
// PIMPL keeps miniaudio types and the third-party header out of public headers.
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(MiniAudioEngine, public AudioSystem)
#pragma region METHOD
public:
	MiniAudioEngine();
	VIRTUAL ~MiniAudioEngine() OVERRIDE;

	VIRTUAL void METHOD(Init)() OVERRIDE;
	VIRTUAL void METHOD(Shutdown)() OVERRIDE;
	VIRTUAL void METHOD(Update)(Float32 dt) OVERRIDE;

	VIRTUAL void METHOD(PlayBGM)(CONST String& path, Bool loop = true) OVERRIDE;
	VIRTUAL void METHOD(StopBGM)() OVERRIDE;
	VIRTUAL void METHOD(PauseBGM)() OVERRIDE;
	VIRTUAL void METHOD(ResumeBGM)() OVERRIDE;
	VIRTUAL Bool METHOD(IsBGMPlaying)() CONST OVERRIDE;
	VIRTUAL Float32 METHOD(GetBGMPlaybackPosition)() CONST OVERRIDE;
	VIRTUAL Float32 METHOD(GetBGMDuration)() CONST OVERRIDE;

	VIRTUAL void METHOD(PlaySFX)(CONST String& path) OVERRIDE;
	VIRTUAL void METHOD(PlaySFXFromClip)(AudioClip* clip, Float32 volume = 1.0f) OVERRIDE;
	VIRTUAL void METHOD(StopAllSFX)() OVERRIDE;

	VIRTUAL void METHOD(SetMasterVolume)(Float32 vol) OVERRIDE;
	VIRTUAL void METHOD(SetBGMVolume)(Float32 vol) OVERRIDE;
	VIRTUAL void METHOD(SetSFXVolume)(Float32 vol) OVERRIDE;
	VIRTUAL Float32 METHOD(GetMasterVolume)() CONST OVERRIDE;

	VIRTUAL AudioClip* METHOD(DecodeClip)(CONST String& path) OVERRIDE;
	VIRTUAL void METHOD(FreeClip)(AudioClip* clip) OVERRIDE;

	VIRTUAL void METHOD(PauseAll)() OVERRIDE;
	VIRTUAL void METHOD(ResumeAll)() OVERRIDE;
	VIRTUAL void METHOD(StopAll)() OVERRIDE;

protected:
private:
	struct Impl;
	std::unique_ptr<Impl> impl;

	MiniAudioEngine(CONST MiniAudioEngine&) MYDELETE;
	MiniAudioEngine& operator=(CONST MiniAudioEngine&) MYDELETE;
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Desktop
MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _MINI_AUDIO_ENGINE_
