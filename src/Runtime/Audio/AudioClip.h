#pragma once
#ifndef _AUDIO_CLIP_
#define _AUDIO_CLIP_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)

// PCM audio data + metadata. Pure data struct, no behavior.
// Owned by AudioManager's ResourceRegistry<AudioClip>.
MYRENDERER_BEGIN_STRUCT(AudioClip)
	Vector<UInt8> pcm_data;           // decoded PCM samples
	UInt32 sample_rate = 0;           // Hz
	UInt32 channels = 0;              // 1=mono, 2=stereo
	UInt32 bits_per_sample = 0;       // 8/16/24/32
	Float64 duration_seconds = 0.0;
	String source_path;
	Bool is_valid = false;
MYRENDERER_END_STRUCT

MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _AUDIO_CLIP_
