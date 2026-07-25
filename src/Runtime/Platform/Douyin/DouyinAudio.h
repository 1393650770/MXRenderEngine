#pragma once
#ifndef _DOUYIN_AUDIO_
#define _DOUYIN_AUDIO_

#if PLATFORM_GLES3

#include "Audio/AudioSystem.h"
#include <emscripten.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)
MYRENDERER_BEGIN_NAMESPACE(Douyin)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(DouyinAudio, public AudioSystem)
#pragma region METHOD
public:
	DouyinAudio() MYDEFAULT;
	VIRTUAL void METHOD(PlayBGM)(CONST String& path, Bool loop) OVERRIDE FINAL
	{
		StopBGM();
		EM_ASM({
			var src = UTF8ToString($0); var loop = !!$1;
			if (typeof tt !== 'undefined' && tt.createInnerAudioContext) {
				var audio = tt.createInnerAudioContext();
				audio.src = src; audio.loop = loop;
				audio.play();
				Module._bgm_audio = audio;
			}
		}, path.c_str(), (int)loop);
	}

	VIRTUAL void METHOD(StopBGM)() OVERRIDE FINAL
	{
		EM_ASM({ if (Module._bgm_audio) { Module._bgm_audio.stop(); Module._bgm_audio.destroy(); Module._bgm_audio = null; } });
	}

	VIRTUAL void METHOD(PlaySFX)(CONST String& path) OVERRIDE FINAL
	{
		EM_ASM({
			var src = UTF8ToString($0);
			if (typeof tt !== 'undefined' && tt.createInnerAudioContext) {
				var sfx = tt.createInnerAudioContext();
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

MYRENDERER_END_NAMESPACE  // Douyin
MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _DOUYIN_AUDIO_
