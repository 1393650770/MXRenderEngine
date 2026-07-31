#pragma once
#ifndef _PLATFORM_FILE_
#define _PLATFORM_FILE_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Platform)

// Platform file I/O HAL. The legacy BasicFileSystem is a stub (OpenFile
// returns null) - save/load must use these. All paths are UTF-8 on Windows.
namespace PlatformFile
{
	// Atomic write: writes path + ".tmp" then renames over the target.
	// Windows uses MoveFileExW (true atomic replace); other platforms fall
	// back to std::filesystem::rename (documented non-atomic).
	Bool METHOD(WriteFileAtomic)(CONST String& path, CONST Vector<UInt8>& data);

	// Reads the whole file into out. Returns false on any failure.
	Bool METHOD(ReadFile)(CONST String& path, Vector<UInt8>& out);

	// Returns true if the file exists.
	Bool METHOD(FileExists)(CONST String& path);
}

MYRENDERER_END_NAMESPACE  // Platform
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _PLATFORM_FILE_