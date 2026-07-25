#pragma once
#ifndef _PLATFORM_AUDIO_
#define _PLATFORM_AUDIO_

// Platform Audio — thin include forwarder to Audio module
#include "Audio/AudioSystem.h"

// Backward compat: PlatformAudio is now Audio::AudioSystem
namespace MXRender { namespace Platform {
	using PlatformAudio = MXRender::Audio::AudioSystem;
} }

#endif // _PLATFORM_AUDIO_
