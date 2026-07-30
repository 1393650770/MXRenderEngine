#include "Audio/AudioManager.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)

// =========================================================================
// Singleton lifecycle
// =========================================================================

AudioManager* AudioManager::s_instance = nullptr;

AudioManager& AudioManager::Get()
{
	CHECK_WITH_LOG(s_instance == nullptr, "AudioManager: Create must be called before Get");
	return *s_instance;
}

Bool AudioManager::IsCreated()
{
	return s_instance != nullptr;
}

void AudioManager::Create(AudioSystem* backend)
{
	CHECK_WITH_LOG(backend == nullptr, "AudioManager: backend cannot be null");
	CHECK_WITH_LOG(s_instance != nullptr, "AudioManager: already created");

	auto* instance = new AudioManager();
	instance->m_backend = backend;
	try
	{
		instance->m_backend->Init();
		s_instance = instance;
	}
	catch (...)
	{
		delete instance->m_backend;
		delete instance;
		throw;
	}
}

void AudioManager::Destroy()
{
	if (!s_instance) return;

	// Free all registered clips via ForEach
	if (s_instance->m_backend)
	{
		// Collect handles first, then free (avoids iterator invalidation)
		Vector<GenericHandle> to_free;
		s_instance->m_clip_registry.ForEach([&to_free](GenericHandle h, AudioClip*) {
			to_free.push_back(h);
		});
		for (auto& h : to_free)
		{
			AudioClip* clip = s_instance->m_clip_registry.Free(h);
			if (clip) s_instance->m_backend->FreeClip(clip);
		}

		s_instance->m_backend->Shutdown();
		delete s_instance->m_backend;
		s_instance->m_backend = nullptr;
	}

	delete s_instance;
	s_instance = nullptr;
}

// =========================================================================
// Per-frame
// =========================================================================

void AudioManager::Update(Float32 dt)
{
	if (m_backend) m_backend->Update(dt);
}

// =========================================================================
// BGM
// =========================================================================

void AudioManager::PlayBGM(CONST String& path, Bool loop)
{
	if (m_backend) m_backend->PlayBGM(path, loop);
}

void AudioManager::StopBGM()
{
	if (m_backend) m_backend->StopBGM();
}

void AudioManager::PauseBGM()
{
	if (m_backend) m_backend->PauseBGM();
}

void AudioManager::ResumeBGM()
{
	if (m_backend) m_backend->ResumeBGM();
}

Bool AudioManager::IsBGMPlaying() CONST
{
	return m_backend ? m_backend->IsBGMPlaying() : false;
}

Float32 AudioManager::GetBGMPlaybackPosition() CONST
{
	return m_backend ? m_backend->GetBGMPlaybackPosition() : 0.0f;
}

Float32 AudioManager::GetBGMDuration() CONST
{
	return m_backend ? m_backend->GetBGMDuration() : 0.0f;
}

// =========================================================================
// SFX
// =========================================================================

void AudioManager::PlaySFX(CONST String& path)
{
	if (m_backend) m_backend->PlaySFX(path);
}

void AudioManager::PlaySFX(AudioClipHandle clip, Float32 volume)
{
	if (!m_backend || !clip.IsValid()) return;
	AudioClip* raw = m_clip_registry.Resolve(clip.value);
	if (raw) m_backend->PlaySFXFromClip(raw, volume);
}

void AudioManager::StopAllSFX()
{
	if (m_backend) m_backend->StopAllSFX();
}

// =========================================================================
// Volume
// =========================================================================

void AudioManager::SetMasterVolume(Float32 vol)
{
	if (m_backend) m_backend->SetMasterVolume(vol);
}

void AudioManager::SetBGMVolume(Float32 vol)
{
	if (m_backend) m_backend->SetBGMVolume(vol);
}

void AudioManager::SetSFXVolume(Float32 vol)
{
	if (m_backend) m_backend->SetSFXVolume(vol);
}

Float32 AudioManager::GetMasterVolume() CONST
{
	return m_backend ? m_backend->GetMasterVolume() : 1.0f;
}

// =========================================================================
// Resource management
// =========================================================================

AudioClipHandle AudioManager::LoadClip(CONST String& path)
{
	if (!m_backend) return {};
	AudioClip* raw = m_backend->DecodeClip(path);
	if (!raw) return {};
	raw->source_path = path;
	GenericHandle h = m_clip_registry.Allocate(raw, path);
	AudioClipHandle handle;
	handle.value = h;
	return handle;
}

AudioClipHandle AudioManager::ImportClip(AudioClip* clip)
{
	if (!clip || !clip->is_valid) return {};
	GenericHandle h = m_clip_registry.Allocate(clip, clip->source_path);
	AudioClipHandle handle;
	handle.value = h;
	return handle;
}

void AudioManager::UnloadClip(AudioClipHandle clip)
{
	if (!m_backend || !clip.IsValid()) return;
	AudioClip* raw = m_clip_registry.Free(clip.value);
	if (raw) m_backend->FreeClip(raw);
}

AudioClip* AudioManager::GetClip(AudioClipHandle clip) CONST
{
	return clip.IsValid() ? m_clip_registry.Resolve(clip.value) : nullptr;
}

// =========================================================================
// Global state
// =========================================================================

void AudioManager::PauseAll()
{
	if (m_backend) m_backend->PauseAll();
}

void AudioManager::ResumeAll()
{
	if (m_backend) m_backend->ResumeAll();
}

void AudioManager::StopAll()
{
	if (m_backend) m_backend->StopAll();
}

MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender
