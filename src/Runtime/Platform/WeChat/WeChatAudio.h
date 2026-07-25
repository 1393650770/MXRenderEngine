#pragma once
#ifndef _WECHAT_AUDIO_
#define _WECHAT_AUDIO_

#if PLATFORM_GLES3

#include "Audio/AudioSystem.h"
#include <emscripten.h>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)
MYRENDERER_BEGIN_NAMESPACE(WeChat)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(WeChatAudio, public AudioSystem)
#pragma region METHOD
public:
	WeChatAudio() MYDEFAULT;
	VIRTUAL ~WeChatAudio() MYDEFAULT;

	VIRTUAL void METHOD(PlayBGM)(CONST String& path, Bool loop) OVERRIDE FINAL
	{
		StopBGM();
		EM_ASM({
			var src = UTF8ToString($0); var loop = !!$1;
			if (typeof wx !== 'undefined' && wx.createInnerAudioContext) {
				var audio = wx.createInnerAudioContext();
				audio.src = src; audio.loop = loop;
				audio.onError(function(e){console.log('[WeChatAudio] BGM error:',e);});
				audio.play();
				Module._bgm_audio = audio;
				console.log('[WeChatAudio] BGM playing:', src);
			}
		}, path.c_str(), (int)loop);
	}

	VIRTUAL void METHOD(StopBGM)() OVERRIDE FINAL
	{
		EM_ASM({
			if (Module._bgm_audio) { Module._bgm_audio.stop(); Module._bgm_audio.destroy(); Module._bgm_audio = null; }
		});
	}

	VIRTUAL void METHOD(PlaySFX)(CONST String& path) OVERRIDE FINAL
	{
		EM_ASM({
			var src = UTF8ToString($0);
			if (typeof wx !== 'undefined' && wx.createInnerAudioContext) {
				var sfx = wx.createInnerAudioContext();
				sfx.src = src; sfx.loop = false;
				sfx.onEnded(function(){ sfx.destroy(); });
				sfx.play();
			}
		}, path.c_str());
	}

	VIRTUAL void METHOD(SetVolume)(Float32 vol) OVERRIDE FINAL
	{
		float v = (vol < 0) ? 0 : (vol > 1 ? 1 : vol);
		EM_ASM({ if (Module._bgm_audio) Module._bgm_audio.volume = $0; }, v);
	}

protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // WeChat
MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _WECHAT_AUDIO_
