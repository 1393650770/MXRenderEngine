#include "Core/ConfigManager.h"
#include "Platform/PlatformFile.h"
#include <filesystem>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Core)

ConfigManager* ConfigManager::s_instance = nullptr;

ConfigManager& ConfigManager::Get()
{
	return *s_instance;
}

void ConfigManager::Create()
{
	if (!s_instance)
		s_instance = new ConfigManager();
}

void ConfigManager::Destroy()
{
	delete s_instance;
	s_instance = nullptr;
}

void ConfigManager::LoadConfig(CONST String& path)
{
	config_path_ = path;
	Vector<UInt8> bytes;
	if (!Platform::PlatformFile::ReadFile(path, bytes))
		return;
	try
	{
		root_ = nlohmann::json::parse(String(bytes.begin(), bytes.end()));
	}
	catch (...)
	{
		root_ = nlohmann::json::object();
	}
	std::error_code ec;
	last_write_time_ = (Float64)std::filesystem::last_write_time(path, ec).time_since_epoch().count();
}

void ConfigManager::SaveConfig(CONST String& path)
{
	String json_str = root_.dump(2);
	Vector<UInt8> bytes(json_str.begin(), json_str.end());
	Platform::PlatformFile::WriteFileAtomic(path, bytes);
}

void ConfigManager::SetReloadCallback(std::function<void()> callback)
{
	reload_callback_ = std::move(callback);
}

void ConfigManager::PollHotReload(Float32 interval_seconds)
{
	if (config_path_.empty())
		return;
	poll_timer_ += interval_seconds;
	if (poll_timer_ < 1.0f)
		return;
	poll_timer_ = 0.0f;

	std::error_code ec;
	Float64 mtime = (Float64)std::filesystem::last_write_time(config_path_, ec).time_since_epoch().count();
	if (ec || mtime == last_write_time_)
		return;
	last_write_time_ = mtime;
	LoadConfig(config_path_);
	if (reload_callback_)
		reload_callback_();
}

MYRENDERER_END_NAMESPACE  // Core
MYRENDERER_END_NAMESPACE  // MXRender