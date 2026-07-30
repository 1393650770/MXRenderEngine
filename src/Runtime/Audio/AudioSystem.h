#pragma once
#ifndef _AUDIO_SYSTEM_
#define _AUDIO_SYSTEM_

// Audio System Abstraction
// Patterned after UISystem: virtual methods with empty defaults.
// Platform-specific implementations:
//   Desktop:   MiniAudioEngine (src/Runtime/Audio/Desktop/)
//   Emscripten: BrowserAudio   (src/Runtime/Audio/Emscripten/)
//   Douyin:     DouyinAudio    (src/Runtime/Platform/Douyin/)
//   WeChat:     WeChatAudio    (src/Runtime/Platform/WeChat/)
//
// Extensibility: inherit AudioSystem and override only the methods your
// backend supports. All methods are non-pure virtual with empty defaults.

#include "Core/ConstDefine.h"
#include "Audio/AudioHandleTypes.h"
#include "Audio/AudioClip.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)

MYRENDERER_BEGIN_CLASS(AudioSystem)
#pragma region METHOD
public:
	AudioSystem() MYDEFAULT;
	VIRTUAL ~AudioSystem() MYDEFAULT;

	// ---- Lifecycle ----
	VIRTUAL void METHOD(Init)() {}
	VIRTUAL void METHOD(Shutdown)() {}
	VIRTUAL void METHOD(Update)(Float32 dt) { (void)dt; }

	// ---- BGM (background music) ----
	VIRTUAL void METHOD(PlayBGM)(CONST String& path, Bool loop = true) { (void)path; (void)loop; }
	VIRTUAL void METHOD(StopBGM)() {}
	VIRTUAL void METHOD(PauseBGM)() {}
	VIRTUAL void METHOD(ResumeBGM)() {}
	VIRTUAL Bool METHOD(IsBGMPlaying)() CONST { return false; }
	VIRTUAL Float32 METHOD(GetBGMPlaybackPosition)() CONST { return 0.0f; }
	VIRTUAL Float32 METHOD(GetBGMDuration)() CONST { return 0.0f; }

	// ---- SFX (sound effects) ----
	VIRTUAL void METHOD(PlaySFX)(CONST String& path) { (void)path; }
	VIRTUAL void METHOD(PlaySFXFromClip)(AudioClip* clip, Float32 volume = 1.0f) { (void)clip; (void)volume; }
	VIRTUAL void METHOD(StopAllSFX)() {}

	// ---- Volume (layered: master x group) ----
	// SetVolume is retained for existing web backends. New backends should
	// override SetMasterVolume; its default forwards to SetVolume.
	VIRTUAL void METHOD(SetVolume)(Float32 vol) { (void)vol; }
	VIRTUAL void METHOD(SetMasterVolume)(Float32 vol) { SetVolume(vol); }
	VIRTUAL void METHOD(SetBGMVolume)(Float32 vol) { (void)vol; }
	VIRTUAL void METHOD(SetSFXVolume)(Float32 vol) { (void)vol; }
	VIRTUAL Float32 METHOD(GetMasterVolume)() CONST { return 1.0f; }

	// ---- Resource management (backend-specific decoding) ----
	VIRTUAL AudioClip* METHOD(DecodeClip)(CONST String& path) { (void)path; return nullptr; }
	VIRTUAL void METHOD(FreeClip)(AudioClip* clip) { (void)clip; }

	// ---- Global state ----
	VIRTUAL void METHOD(PauseAll)() {}
	VIRTUAL void METHOD(ResumeAll)() {}
	VIRTUAL void METHOD(StopAll)() {}

protected:
private:
	AudioSystem(CONST AudioSystem&) MYDELETE;
	AudioSystem& operator=(CONST AudioSystem&) MYDELETE;
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _AUDIO_SYSTEM_
