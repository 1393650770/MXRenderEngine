#include "Audio/Desktop/MiniAudioEngine.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <algorithm>
#include <cstring>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)
MYRENDERER_BEGIN_NAMESPACE(Desktop)

namespace
{
	Float32 ClampVolume(Float32 volume)
	{
		return std::clamp(volume, 0.0f, 1.0f);
	}
}

struct MiniAudioEngine::Impl
{
	MYRENDERER_BEGIN_STRUCT(ActiveSound)
		ma_sound sound{};
		ma_audio_buffer buffer{};
		Vector<UInt8> pcm_copy;
		Float32 instance_volume = 1.0f;
		Bool owns_buffer = false;
		Bool paused = false;
	MYRENDERER_END_STRUCT

	ma_engine engine{};
	Bool initialized = false;
	ma_sound bgm{};
	Bool bgm_initialized = false;
	Bool bgm_paused = false;
	Vector<std::unique_ptr<ActiveSound>> active_sfx;
	Float32 master_volume = 1.0f;
	Float32 bgm_volume = 1.0f;
	Float32 sfx_volume = 1.0f;

	void ApplyBGMVolume()
	{
		if (bgm_initialized)
			ma_sound_set_volume(&bgm, master_volume * bgm_volume);
	}

	void ApplySFXVolume(ActiveSound& sound)
	{
		ma_sound_set_volume(&sound.sound,
			master_volume * sfx_volume * sound.instance_volume);
	}

	void DestroySound(ActiveSound& sound)
	{
		ma_sound_stop(&sound.sound);
		ma_sound_uninit(&sound.sound);
		if (sound.owns_buffer)
			ma_audio_buffer_uninit(&sound.buffer);
	}

	void StopBGM()
	{
		if (!bgm_initialized)
			return;
		ma_sound_stop(&bgm);
		ma_sound_uninit(&bgm);
		bgm_initialized = false;
		bgm_paused = false;
	}

	void StopAllSFX()
	{
		for (auto& sound : active_sfx)
			DestroySound(*sound);
		active_sfx.clear();
	}
};

MiniAudioEngine::MiniAudioEngine()
	: impl(std::make_unique<Impl>())
{
}

MiniAudioEngine::~MiniAudioEngine()
{
	Shutdown();
}

void MiniAudioEngine::Init()
{
	if (impl->initialized)
		return;

	ma_result result = ma_engine_init(nullptr, &impl->engine);
	CHECK_WITH_LOG(result != MA_SUCCESS, "MiniAudioEngine: failed to initialize audio engine");
	impl->initialized = true;
}

void MiniAudioEngine::Shutdown()
{
	if (!impl || !impl->initialized)
		return;

	impl->StopBGM();
	impl->StopAllSFX();
	ma_engine_uninit(&impl->engine);
	impl->initialized = false;
}

void MiniAudioEngine::Update(Float32 dt)
{
	(void)dt;
	if (!impl->initialized)
		return;

	for (auto it = impl->active_sfx.begin(); it != impl->active_sfx.end(); )
	{
		Impl::ActiveSound& sound = **it;
		if (!sound.paused && ma_sound_at_end(&sound.sound))
		{
			impl->DestroySound(sound);
			it = impl->active_sfx.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void MiniAudioEngine::PlayBGM(CONST String& path, Bool loop)
{
	if (!impl->initialized)
		Init();

	impl->StopBGM();
	ma_result result = ma_sound_init_from_file(&impl->engine, path.c_str(),
		MA_SOUND_FLAG_STREAM, nullptr, nullptr, &impl->bgm);
	CHECK_WITH_LOG(result != MA_SUCCESS, "MiniAudioEngine: failed to load BGM");

	impl->bgm_initialized = true;
	ma_sound_set_looping(&impl->bgm, loop ? MA_TRUE : MA_FALSE);
	impl->ApplyBGMVolume();
	result = ma_sound_start(&impl->bgm);
	CHECK_WITH_LOG(result != MA_SUCCESS, "MiniAudioEngine: failed to start BGM");
}

void MiniAudioEngine::StopBGM()
{
	impl->StopBGM();
}

void MiniAudioEngine::PauseBGM()
{
	if (!impl->bgm_initialized || impl->bgm_paused)
		return;
	ma_sound_stop(&impl->bgm);
	impl->bgm_paused = true;
}

void MiniAudioEngine::ResumeBGM()
{
	if (!impl->bgm_initialized || !impl->bgm_paused)
		return;
	ma_sound_start(&impl->bgm);
	impl->bgm_paused = false;
}

Bool MiniAudioEngine::IsBGMPlaying() CONST
{
	return impl->bgm_initialized && ma_sound_is_playing(&impl->bgm) != MA_FALSE;
}

Float32 MiniAudioEngine::GetBGMPlaybackPosition() CONST
{
	if (!impl->bgm_initialized)
		return 0.0f;
	Float32 cursor = 0.0f;
	return ma_sound_get_cursor_in_seconds(&impl->bgm, &cursor) == MA_SUCCESS ? cursor : 0.0f;
}

Float32 MiniAudioEngine::GetBGMDuration() CONST
{
	if (!impl->bgm_initialized)
		return 0.0f;
	Float32 length = 0.0f;
	return ma_sound_get_length_in_seconds(&impl->bgm, &length) == MA_SUCCESS ? length : 0.0f;
}

void MiniAudioEngine::PlaySFX(CONST String& path)
{
	if (!impl->initialized)
		Init();

	auto sound = std::make_unique<Impl::ActiveSound>();
	ma_result result = ma_sound_init_from_file(&impl->engine, path.c_str(),
		MA_SOUND_FLAG_DECODE, nullptr, nullptr, &sound->sound);
	CHECK_WITH_LOG(result != MA_SUCCESS, "MiniAudioEngine: failed to load SFX");

	impl->ApplySFXVolume(*sound);
	result = ma_sound_start(&sound->sound);
	if (result != MA_SUCCESS)
	{
		ma_sound_uninit(&sound->sound);
		CHECK_WITH_LOG(true, "MiniAudioEngine: failed to start SFX");
	}
	impl->active_sfx.push_back(std::move(sound));
}

void MiniAudioEngine::PlaySFXFromClip(AudioClip* clip, Float32 volume)
{
	if (!clip || !clip->is_valid || clip->pcm_data.empty() ||
		clip->channels == 0 || clip->sample_rate == 0 || clip->bits_per_sample != 32)
		return;
	if (!impl->initialized)
		Init();

	auto sound = std::make_unique<Impl::ActiveSound>();
	sound->pcm_copy = clip->pcm_data;
	sound->instance_volume = ClampVolume(volume);
	ma_audio_buffer_config config = ma_audio_buffer_config_init(
		ma_format_f32, clip->channels,
		sound->pcm_copy.size() / (sizeof(Float32) * clip->channels),
		sound->pcm_copy.data(), nullptr);
	config.sampleRate = clip->sample_rate;

	ma_result result = ma_audio_buffer_init(&config, &sound->buffer);
	CHECK_WITH_LOG(result != MA_SUCCESS, "MiniAudioEngine: failed to initialize clip buffer");
	sound->owns_buffer = true;

	result = ma_sound_init_from_data_source(&impl->engine, &sound->buffer,
		0, nullptr, &sound->sound);
	if (result != MA_SUCCESS)
	{
		ma_audio_buffer_uninit(&sound->buffer);
		CHECK_WITH_LOG(true, "MiniAudioEngine: failed to initialize clip sound");
	}

	impl->ApplySFXVolume(*sound);
	result = ma_sound_start(&sound->sound);
	if (result != MA_SUCCESS)
	{
		impl->DestroySound(*sound);
		CHECK_WITH_LOG(true, "MiniAudioEngine: failed to start clip sound");
	}
	impl->active_sfx.push_back(std::move(sound));
}

void MiniAudioEngine::StopAllSFX()
{
	impl->StopAllSFX();
}

void MiniAudioEngine::SetMasterVolume(Float32 vol)
{
	impl->master_volume = ClampVolume(vol);
	impl->ApplyBGMVolume();
	for (auto& sound : impl->active_sfx)
		impl->ApplySFXVolume(*sound);
}

void MiniAudioEngine::SetBGMVolume(Float32 vol)
{
	impl->bgm_volume = ClampVolume(vol);
	impl->ApplyBGMVolume();
}

void MiniAudioEngine::SetSFXVolume(Float32 vol)
{
	impl->sfx_volume = ClampVolume(vol);
	for (auto& sound : impl->active_sfx)
		impl->ApplySFXVolume(*sound);
}

Float32 MiniAudioEngine::GetMasterVolume() CONST
{
	return impl->master_volume;
}

AudioClip* MiniAudioEngine::DecodeClip(CONST String& path)
{
	ma_decoder decoder{};
	ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
	ma_result result = ma_decoder_init_file(path.c_str(), &config, &decoder);
	if (result != MA_SUCCESS)
		return nullptr;

	ma_uint64 frame_count = 0;
	result = ma_decoder_get_length_in_pcm_frames(&decoder, &frame_count);
	if (result != MA_SUCCESS || frame_count == 0 || decoder.outputChannels == 0 ||
		decoder.outputSampleRate == 0)
	{
		ma_decoder_uninit(&decoder);
		return nullptr;
	}

	auto* clip = new AudioClip();
	clip->sample_rate = decoder.outputSampleRate;
	clip->channels = decoder.outputChannels;
	clip->bits_per_sample = 32;
	clip->duration_seconds = (Float64)frame_count / (Float64)clip->sample_rate;
	clip->source_path = path;
	clip->pcm_data.resize((size_t)frame_count * clip->channels * sizeof(Float32));

	ma_uint64 frames_read = 0;
	result = ma_decoder_read_pcm_frames(&decoder, clip->pcm_data.data(), frame_count, &frames_read);
	ma_decoder_uninit(&decoder);
	if (result != MA_SUCCESS || frames_read == 0)
	{
		delete clip;
		return nullptr;
	}

	clip->pcm_data.resize((size_t)frames_read * clip->channels * sizeof(Float32));
	clip->duration_seconds = (Float64)frames_read / (Float64)clip->sample_rate;
	clip->is_valid = true;
	return clip;
}

void MiniAudioEngine::FreeClip(AudioClip* clip)
{
	delete clip;
}

void MiniAudioEngine::PauseAll()
{
	PauseBGM();
	for (auto& sound : impl->active_sfx)
	{
		if (ma_sound_is_playing(&sound->sound) != MA_FALSE)
		{
			ma_sound_stop(&sound->sound);
			sound->paused = true;
		}
	}
}

void MiniAudioEngine::ResumeAll()
{
	ResumeBGM();
	for (auto& sound : impl->active_sfx)
	{
		if (sound->paused)
		{
			ma_sound_start(&sound->sound);
			sound->paused = false;
		}
	}
}

void MiniAudioEngine::StopAll()
{
	impl->StopBGM();
	impl->StopAllSFX();
}

MYRENDERER_END_NAMESPACE  // Desktop
MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender
