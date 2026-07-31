#include "World/Save/SaveManager.h"
#include "World/Save/WorldSave.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

void SaveManager::SetSaveProvider(std::function<void(WorldSave&)> save_provider,
	std::function<Bool(CONST WorldSave&)> load_consumer)
{
	save_provider_ = std::move(save_provider);
	load_consumer_ = std::move(load_consumer);
}

void SaveManager::SetMigrationHook(std::function<Bool(WorldSave&, UInt32)> hook)
{
	migration_hook_ = std::move(hook);
}

Bool SaveManager::SaveGame(CONST String& path)
{
	if (!save_provider_)
		return false;
	WorldSave save;
	save_provider_(save);
	SaveWorldToFile(save, path);
	return true;
}

Bool SaveManager::LoadGame(CONST String& path)
{
	if (!load_consumer_)
		return false;
	WorldSave save;
	if (!LoadWorldFromFile(path, save))
		return false;

	// Version migration.
	if (migration_hook_ && save.format_version < 1)
	{
		if (!migration_hook_(save, save.format_version))
			return false;
	}
	return load_consumer_(save);
}

void SaveManager::TickAutoSave(Float32 dt)
{
	if (auto_save_interval_ <= 0.0f)
		return;
	auto_save_timer_ += dt;
	if (auto_save_timer_ >= auto_save_interval_)
	{
		auto_save_timer_ = 0.0f;
		has_pending_auto_save_ = true;
		// Auto-save writes a .tmp so a crash mid-write never corrupts the
		// real save; the game decides when to promote it.
		WorldSave save;
		if (save_provider_)
		{
			save_provider_(save);
			SaveWorldToFile(save, "auto_save.tmp");
		}
	}
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender