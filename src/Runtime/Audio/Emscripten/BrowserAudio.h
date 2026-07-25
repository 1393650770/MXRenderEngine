#pragma once
#ifndef _BROWSER_AUDIO_
#define _BROWSER_AUDIO_

#if PLATFORM_GLES3

#include "Audio/AudioSystem.h"
#include <emscripten.h>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)
MYRENDERER_BEGIN_NAMESPACE(Emscripten)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(BrowserAudio, public AudioSystem)
#pragma region METHOD
public:
	BrowserAudio() MYDEFAULT;
	VIRTUAL ~BrowserAudio() MYDEFAULT;

	VIRTUAL void METHOD(PlayBGM)(CONST String& path, Bool loop) OVERRIDE FINAL;
	VIRTUAL void METHOD(StopBGM)() OVERRIDE FINAL;
	VIRTUAL void METHOD(PlaySFX)(CONST String& path) OVERRIDE FINAL;
	VIRTUAL void METHOD(SetVolume)(Float32 vol) OVERRIDE FINAL;
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

// ---- Inline implementations ----

inline void BrowserAudio::PlayBGM(CONST String& path, Bool loop)
{
	StopBGM();
	EM_ASM({
		var src = UTF8ToString($0); var loop = !!$1; var audio = null;
		if (typeof Audio !== 'undefined') { audio = new Audio(src); audio.loop = loop; audio.play().catch(function(){}); }
		else if (typeof wx !== 'undefined' && wx.createInnerAudioContext) { audio = wx.createInnerAudioContext(); audio.src = src; audio.loop = loop; audio.play(); }
		else if (typeof tt !== 'undefined' && tt.createInnerAudioContext) { audio = tt.createInnerAudioContext(); audio.src = src; audio.loop = loop; audio.play(); }
		if (audio) Module._bgm_audio = audio;
	}, path.c_str(), (int)loop);
}

inline void BrowserAudio::StopBGM()
{
	EM_ASM({ if (Module._bgm_audio) { Module._bgm_audio.pause(); if (Module._bgm_audio.destroy) Module._bgm_audio.destroy(); delete Module._bgm_audio; Module._bgm_audio = null; } });
}

inline void BrowserAudio::PlaySFX(CONST String& path)
{
	EM_ASM({
		var src = UTF8ToString($0);
		var audio = null;
		if (typeof Audio !== 'undefined') { audio = new Audio(src); audio.play().catch(function(){}); }
		else if (typeof wx !== 'undefined' && wx.createInnerAudioContext) { audio = wx.createInnerAudioContext(); audio.src = src; audio.loop = false; audio.play(); }
		else if (typeof tt !== 'undefined' && tt.createInnerAudioContext) { audio = tt.createInnerAudioContext(); audio.src = src; audio.loop = false; audio.play(); }
	}, path.c_str());
}

inline void BrowserAudio::SetVolume(Float32 vol)
{
	float v = (vol < 0) ? 0 : (vol > 1 ? 1 : vol);
	EM_ASM({ if (Module._bgm_audio) Module._bgm_audio.volume = $0; }, v);
}

MYRENDERER_END_NAMESPACE  // Emscripten
MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _BROWSER_AUDIO_
