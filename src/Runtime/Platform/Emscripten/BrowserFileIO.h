#pragma once
#ifndef _BROWSER_FILEIO_
#define _BROWSER_FILEIO_

#if PLATFORM_GLES3

#include "Platform/PlatformFileIO.h"
#include <cstdio>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Platform)
MYRENDERER_BEGIN_NAMESPACE(Emscripten)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(BrowserFileIO, public PlatformFileIO)
#pragma region METHOD
public:
	BrowserFileIO() MYDEFAULT;
	VIRTUAL ~BrowserFileIO() MYDEFAULT;

	VIRTUAL void   METHOD(Init)() OVERRIDE FINAL;
	VIRTUAL void   METHOD(SyncPersistent)() OVERRIDE FINAL;
	VIRTUAL Bool   METHOD(ReadFile)(CONST String& path, Vector<UInt8>& out_data) OVERRIDE FINAL;
	VIRTUAL Bool   METHOD(WriteFile)(CONST String& path, const void* data, UInt32 size) OVERRIDE FINAL;
	VIRTUAL Bool   METHOD(FileExists)(CONST String& path) CONST OVERRIDE FINAL;
	VIRTUAL UInt32 METHOD(GetFileSize)(CONST String& path) CONST OVERRIDE FINAL;
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Emscripten
MYRENDERER_END_NAMESPACE  // Platform
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _BROWSER_FILEIO_
