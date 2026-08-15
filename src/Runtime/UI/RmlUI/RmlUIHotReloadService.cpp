#include "RmlUIHotReloadService.h"
#include "Platform/PlatformFile.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(RmlUI)

RmlUIHotReloadService::RmlUIHotReloadService()
	: m_source_root(FindProjectResourceRoot())
{
	if (m_source_root.empty())
		std::cout << "[UIHotReload] project resource root not found (cwd is not inside the project) — watching output dir only" << std::endl;
}

void RmlUIHotReloadService::TrackPath(const String& rel_path)
{
	// dirname: everything before the last '/' (or '\').
	auto sep = rel_path.find_last_of("/\\");
	const String dir = sep == String::npos ? String() : rel_path.substr(0, sep);
	if (std::find(m_tracked_dirs.begin(), m_tracked_dirs.end(), dir) == m_tracked_dirs.end())
		m_tracked_dirs.push_back(dir);

	std::error_code ec;
	FileStamp st;

	// Output should match the source at (re)load time: xmake's incremental
	// resource copy is unreliable, so a stale output file could otherwise be
	// loaded at startup. Content-compare (files are tiny) and re-sync.
	if (!m_source_root.empty())
	{
		const String src_path = m_source_root + "/resource/" + rel_path;
		Vector<UInt8> src_bytes;
		if (Platform::PlatformFile::ReadFile(src_path, src_bytes))
		{
			Vector<UInt8> out_bytes;
			if (!Platform::PlatformFile::ReadFile(rel_path, out_bytes) || out_bytes != src_bytes)
			{
				Platform::PlatformFile::WriteFileAtomic(rel_path, src_bytes);
				std::cout << "[UIHotReload] synced " << src_path << " -> " << rel_path << " at load" << std::endl;
			}
			st.source = (Float64)std::filesystem::last_write_time(src_path, ec).time_since_epoch().count();
			if (ec) st.source = 0.0;
			ec.clear();
		}
	}

	// Seed the output stamp so the next poll never fires on the file as-is.
	st.output = (Float64)std::filesystem::last_write_time(rel_path, ec).time_since_epoch().count();
	if (ec) st.output = 0.0;
	ec.clear();

	m_stamps[rel_path] = st;
}

UIHotReloadChangeSet RmlUIHotReloadService::Poll(Float32 dt)
{
	UIHotReloadChangeSet out;
	if (!m_enabled) return out;

	m_timer += dt;
	if (m_timer < kPollInterval) return out;
	m_timer = 0.0f;

	for (const auto& dir : m_tracked_dirs)
		ScanDirectory(dir, out);
	return out;
}

void RmlUIHotReloadService::ScanDirectory(const String& dir, UIHotReloadChangeSet& out)
{
	std::error_code ec;
	const String dir_name = dir.empty() ? "." : dir;
	for (const auto& entry : std::filesystem::directory_iterator(dir_name, ec))
	{
		if (ec) break; // directory vanished mid-scan
		if (!std::filesystem::is_regular_file(entry.path(), ec)) { ec.clear(); continue; }

		const String ext = entry.path().extension().string();
		if (ext != ".rml" && ext != ".rcss") continue;

		const String rel = dir.empty() ? entry.path().filename().string()
			: dir + "/" + entry.path().filename().string();
		const String out_path = entry.path().string(); // native separators

		Float64 out_mtime = (Float64)std::filesystem::last_write_time(entry.path(), ec).time_since_epoch().count();
		if (ec) { ec.clear(); continue; }

		auto it = m_stamps.find(rel);
		if (it == m_stamps.end())
		{
			// New file: seed only, never trigger on first sight.
			FileStamp st;
			st.output = out_mtime;
			if (!m_source_root.empty())
			{
				const String src = m_source_root + "/resource/" + rel;
				st.source = (Float64)std::filesystem::last_write_time(src, ec).time_since_epoch().count();
				if (ec) st.source = 0.0;
				ec.clear();
			}
			m_stamps[rel] = st;
			continue;
		}

		FileStamp& st = it->second;

		// Source changed → copy over the output (atomic write) so the loaded
		// file matches the authoritative edit location. Stamps are recorded
		// AFTER a successful copy so a failed write retries next poll.
		if (!m_source_root.empty())
		{
			const String src = m_source_root + "/resource/" + rel;
			Float64 src_mtime = (Float64)std::filesystem::last_write_time(src, ec).time_since_epoch().count();
			if (ec) { ec.clear(); src_mtime = 0.0; }
			if (src_mtime != 0.0 && src_mtime != st.source)
			{
				// Content-compare first: the EDITOR writes both copies itself
				// (UIDesignerState::WriteFiles), so the mtime bump is not a
				// real edit — copying would re-touch the output and restart the
				// reload ping-pong on every designer command.
				Vector<UInt8> bytes, cur;
				const bool src_read = Platform::PlatformFile::ReadFile(src, bytes);
				const bool same = src_read && Platform::PlatformFile::ReadFile(out_path, cur) && cur == bytes;
				if (same)
				{
					st.source = src_mtime;   // already in sync — just re-stamp
				}
				else if (src_read && Platform::PlatformFile::WriteFileAtomic(out_path, bytes))
				{
					std::cout << "[UIHotReload] copied " << src << " -> " << out_path << std::endl;
					out_mtime = (Float64)std::filesystem::last_write_time(entry.path(), ec).time_since_epoch().count();
					if (ec) ec.clear();
					st.source = src_mtime;
				}
			}
		}

		const bool changed = out_mtime != st.output;
		st.output = out_mtime;
		if (!changed) continue;

		if (ext == ".rcss") out.rcss_changed = true;
		else out.rml_changed_paths.push_back(rel);
	}
}

String RmlUIHotReloadService::FindProjectResourceRoot()
{
	std::error_code ec;
	auto dir = std::filesystem::current_path(ec);
	if (ec) return "";

	for (UInt32 level = 0; level < kMaxAncestorLevels; ++level)
	{
		if (std::filesystem::is_directory(dir / "resource" / "RmlUI", ec))
			return dir.string();
		ec.clear();
		if (level + 1 == kMaxAncestorLevels) break;
		dir = dir.parent_path();
		if (dir.empty()) break;
	}
	return "";
}

MYRENDERER_END_NAMESPACE // RmlUI
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
