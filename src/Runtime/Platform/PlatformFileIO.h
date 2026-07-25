#pragma once
#ifndef _PLATFORM_FILEIO_
#define _PLATFORM_FILEIO_

// Platform File I/O Abstraction
// Patterned after UISystem: virtual methods with empty defaults so platforms
// only override what they need. Browser = Emscripten VFS, WeChat = wx FS, etc.

#include "Core/ConstDefine.h"
#include <vector>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Platform)

MYRENDERER_BEGIN_CLASS(PlatformFileIO)
#pragma region METHOD
public:
	PlatformFileIO() MYDEFAULT;
	VIRTUAL ~PlatformFileIO() MYDEFAULT;

	VIRTUAL void   METHOD(Init)() {}
	VIRTUAL void   METHOD(SyncPersistent)() {}
	VIRTUAL Bool   METHOD(ReadFile)(CONST String& path, Vector<UInt8>& out_data) { return false; }
	VIRTUAL Bool   METHOD(WriteFile)(CONST String& path, const void* data, UInt32 size) { return false; }
	VIRTUAL Bool   METHOD(FileExists)(CONST String& path) CONST { return false; }
	VIRTUAL UInt32 METHOD(GetFileSize)(CONST String& path) CONST { return 0; }

	// Path helpers (common across platforms)
	static String METHOD(GetWritablePath)() { return "/persistent/"; }
	static String METHOD(GetBundlePath)()   { return "/bundle/"; }

protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Platform
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _PLATFORM_FILEIO_
