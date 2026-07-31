#include "Platform/PlatformFile.h"
#include <fstream>
#include <filesystem>

#if PLATFORM_WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Platform)

namespace PlatformFile
{
	Bool WriteFileAtomic(CONST String& path, CONST Vector<UInt8>& data)
	{
		String tmp = path + ".tmp";
		{
			std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
			if (!file.is_open())
				return false;
			file.write((const char*)data.data(), (std::streamsize)data.size());
			file.flush();
			if (!file.good())
				return false;
		}

#if PLATFORM_WIN32
		// True atomic replace.
		std::wstring wtmp(tmp.begin(), tmp.end());
		std::wstring wpath(path.begin(), path.end());
		if (!MoveFileExW(wtmp.c_str(), wpath.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			return false;
#else
		std::error_code ec;
		std::filesystem::rename(tmp, path, ec);
		if (ec)
			return false;
#endif
		return true;
	}

	Bool ReadFile(CONST String& path, Vector<UInt8>& out)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file.is_open())
			return false;
		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);
		out.resize((size_t)size);
		if (size > 0)
			file.read((char*)out.data(), size);
		return file.good() || size == 0;
	}

	Bool FileExists(CONST String& path)
	{
		std::error_code ec;
		return std::filesystem::exists(path, ec);
	}
}

MYRENDERER_END_NAMESPACE  // Platform
MYRENDERER_END_NAMESPACE  // MXRender