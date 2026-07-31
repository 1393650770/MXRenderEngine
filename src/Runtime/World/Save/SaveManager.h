#pragma once
#ifndef _SAVE_MANAGER_
#define _SAVE_MANAGER_

#include "Core/ConstDefine.h"
#include "World/Save/WorldSave.h"
#include <functional>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Save/Load orchestration: collects the current world state into a WorldSave
// (via callbacks provided by the game), writes it atomically, and restores
// on load. Auto-save writes auto_save.tmp which the next launch detects.
MYRENDERER_BEGIN_CLASS(SaveManager)
#pragma region METHOD
public:
	SaveManager() MYDEFAULT;
	~SaveManager() MYDEFAULT;

	// Game provides these; SaveManager does not know the world internals.
	void METHOD(SetSaveProvider)(std::function<void(WorldSave&)> save_provider,
		std::function<Bool(CONST WorldSave&)> load_consumer);

	Bool METHOD(SaveGame)(CONST String& path);
	Bool METHOD(LoadGame)(CONST String& path);
	void METHOD(SetAutoSaveInterval)(Float32 seconds) { auto_save_interval_ = seconds; }
	void METHOD(TickAutoSave)(Float32 dt);
	Bool METHOD(HasPendingAutoSave)() CONST { return has_pending_auto_save_; }
	void METHOD(ClearPendingAutoSave)() { has_pending_auto_save_ = false; }

	// Version migration hook (called on load when versions differ).
	void METHOD(SetMigrationHook)(std::function<Bool(WorldSave&, UInt32)> hook);

protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	std::function<void(WorldSave&)> save_provider_;
	std::function<Bool(CONST WorldSave&)> load_consumer_;
	std::function<Bool(WorldSave&, UInt32)> migration_hook_;
	Float32 auto_save_interval_ = 60.0f;
	Float32 auto_save_timer_ = 0.0f;
	Bool has_pending_auto_save_ = false;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _SAVE_MANAGER_