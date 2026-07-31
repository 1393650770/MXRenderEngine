#pragma once
#ifndef _CONFIG_MANAGER_
#define _CONFIG_MANAGER_

#include "Core/ConstDefine.h"
#include <nlohmann/json.hpp>
#include <functional>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Core)

// Typed config facade (nlohmann::json backend). Logic-thread only - no
// locking. Get<T> returns the default on missing key or type mismatch
// (json::value would throw type_error.302).
MYRENDERER_BEGIN_CLASS(ConfigManager)
#pragma region METHOD
public:
	static ConfigManager& METHOD(Get)();
	static void METHOD(Create)();
	static void METHOD(Destroy)();

	template<typename T>
	T METHOD(Get)(CONST String& key, CONST T& default_value) CONST
	{
		if (!root_.contains(key))
			return default_value;
		try
		{
			return root_[key].get<T>();
		}
		catch (CONST nlohmann::json::exception&)
		{
			return default_value;
		}
	}

	template<typename T>
	void METHOD(Set)(CONST String& key, CONST T& value)
	{
		root_[key] = value;
	}

	void METHOD(LoadConfig)(CONST String& path);
	void METHOD(SaveConfig)(CONST String& path);
	void METHOD(SetReloadCallback)(std::function<void()> callback);
	// Polls the config file mtime; reloads + fires the callback on change.
	void METHOD(PollHotReload)(Float32 interval_seconds = 1.0f);

protected:

private:
	ConfigManager() MYDEFAULT;
	~ConfigManager() MYDEFAULT;

#pragma endregion

#pragma region MEMBER
public:

protected:
	static ConfigManager* s_instance;
	nlohmann::json root_;
	String config_path_;
	std::function<void()> reload_callback_;
	Float64 last_write_time_ = 0.0;
	Float32 poll_timer_ = 0.0f;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Core
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _CONFIG_MANAGER_