#pragma once
#ifndef _AUDIO_MANAGER_
#define _AUDIO_MANAGER_

#include "Core/ConstDefine.h"
#include "Core/ResourceRegistry.h"
#include "Audio/AudioSystem.h"
#include "Audio/AudioHandleTypes.h"
#include "Audio/AudioClip.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Audio)

/**
 * Global Audio Manager -- backend-agnostic singleton facade.
 *
 * Mirrors UIManager / ECSManager pattern exactly.  Application layer
 * uses ONLY this class.  The concrete backend (MiniAudioEngine, etc.)
 * is passed via Create() and stored as an AudioSystem*.
 *
 * Usage:
 *   auto* audio = new Audio::Desktop::MiniAudioEngine();
 *   AudioManager::Create(audio);
 *   AudioManager::Get().PlayBGM("bgm.wav", true);
 *   auto clip = AudioManager::Get().LoadClip("sfx.wav");
 *   AudioManager::Get().PlaySFX(clip);
 *   AudioManager::Destroy();
 */
MYRENDERER_BEGIN_CLASS(AudioManager)
#pragma region METHOD
public:
	static AudioManager& METHOD(Get)();
	static Bool METHOD(IsCreated)();
	static void METHOD(Create)(AudioSystem* backend);
	static void METHOD(Destroy)();

	// ---- Per-frame ----
	void METHOD(Update)(Float32 dt);

	// ---- BGM (background music) ----
	void METHOD(PlayBGM)(CONST String& path, Bool loop = true);
	void METHOD(StopBGM)();
	void METHOD(PauseBGM)();
	void METHOD(ResumeBGM)();
	Bool METHOD(IsBGMPlaying)() CONST;
	Float32 METHOD(GetBGMPlaybackPosition)() CONST;
	Float32 METHOD(GetBGMDuration)() CONST;

	// ---- SFX (sound effects) ----
	void METHOD(PlaySFX)(CONST String& path);
	void METHOD(PlaySFX)(AudioClipHandle clip, Float32 volume = 1.0f);
	void METHOD(StopAllSFX)();

	// ---- Volume ----
	void METHOD(SetMasterVolume)(Float32 vol);
	void METHOD(SetBGMVolume)(Float32 vol);
	void METHOD(SetSFXVolume)(Float32 vol);
	Float32 METHOD(GetMasterVolume)() CONST;

	// ---- Resource management ----
	AudioClipHandle METHOD(LoadClip)(CONST String& path);
	// Import a pre-built AudioClip (created by codegen, etc.) into the registry.
	// Ownership transfers to the manager; use UnloadClip to free.
	AudioClipHandle METHOD(ImportClip)(AudioClip* clip);
	void METHOD(UnloadClip)(AudioClipHandle clip);
	AudioClip* METHOD(GetClip)(AudioClipHandle clip) CONST;

	// ---- Global state ----
	void METHOD(PauseAll)();
	void METHOD(ResumeAll)();
	void METHOD(StopAll)();

	// ---- Backend access (for advanced use) ----
	AudioSystem* METHOD(GetBackend)() CONST { return m_backend; }

private:
	AudioManager() MYDEFAULT;
	~AudioManager() MYDEFAULT;

	AudioSystem* m_backend = nullptr;
	static AudioManager* s_instance;

	// Clip registry (Handle to AudioClip* mapping with generation safety)
	ResourceRegistry<AudioClip> m_clip_registry;

	AudioManager(CONST AudioManager&) MYDELETE;
	AudioManager& operator=(CONST AudioManager&) MYDELETE;
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Audio
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _AUDIO_MANAGER_
