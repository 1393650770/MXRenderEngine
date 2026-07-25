#pragma once
#ifndef _DOUYIN_FILEIO_
#define _DOUYIN_FILEIO_

#if PLATFORM_GLES3

#include "Platform/PlatformFileIO.h"
#include <emscripten.h>
#include <cstdio>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Platform)
MYRENDERER_BEGIN_NAMESPACE(Douyin)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(DouyinFileIO, public PlatformFileIO)
#pragma region METHOD
public:
	DouyinFileIO() MYDEFAULT;

	VIRTUAL void METHOD(Init)() OVERRIDE FINAL
	{
		// tt.getFileSystemManager for persistent storage
		EM_ASM({
			if (typeof tt !== 'undefined' && tt.getFileSystemManager) {
				Module._douyin_fs = tt.getFileSystemManager();
				console.log('[DouyinFileIO] FileSystemManager ready');
			}
		});
		// Also mount MEMFS for bundled assets
		EM_ASM( FS.mkdir('/persistent'); );
	}

	VIRTUAL Bool METHOD(ReadFile)(CONST String& path, Vector<UInt8>& out_data) OVERRIDE FINAL
	{
		FILE* f = fopen(path.c_str(), "rb");
		if (!f) return false;
		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		fseek(f, 0, SEEK_SET);
		out_data.resize(size);
		fread(out_data.data(), 1, size, f);
		fclose(f);
		return true;
	}

	VIRTUAL Bool METHOD(FileExists)(CONST String& path) CONST OVERRIDE FINAL
	{
		FILE* f = fopen(path.c_str(), "rb");
		if (f) { fclose(f); return true; }
		return false;
	}

protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Douyin
MYRENDERER_END_NAMESPACE  // Platform
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _DOUYIN_FILEIO_
