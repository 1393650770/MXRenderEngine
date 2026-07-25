#if PLATFORM_GLES3

#include "Platform/Emscripten/BrowserFileIO.h"
#include <emscripten.h>
#include <cstdio>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Platform)
MYRENDERER_BEGIN_NAMESPACE(Emscripten)

void BrowserFileIO::Init()
{
	EM_ASM(
		FS.mkdir('/persistent');
		FS.mount(IDBFS, {}, '/persistent');
		FS.syncfs(true, function(err) {
			if (err) console.log('[BrowserFileIO] IDBFS sync error:', err);
			else console.log('[BrowserFileIO] IDBFS mounted');
		});
	);
	std::cout << "[BrowserFileIO] Initialized: MEMFS + IDBFS" << std::endl;
}

void BrowserFileIO::SyncPersistent()
{
	EM_ASM( FS.syncfs(false, function(err) {}); );
}

Bool BrowserFileIO::ReadFile(CONST String& path, Vector<UInt8>& out_data)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	out_data.resize(size);
	size_t read = fread(out_data.data(), 1, size, f);
	fclose(f);
	return read == (size_t)size;
}

Bool BrowserFileIO::WriteFile(CONST String& path, const void* data, UInt32 size)
{
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) return false;
	size_t written = fwrite(data, 1, size, f);
	fclose(f);
	return written == size;
}

Bool BrowserFileIO::FileExists(CONST String& path) CONST
{
	FILE* f = fopen(path.c_str(), "rb");
	if (f) { fclose(f); return true; }
	return false;
}

UInt32 BrowserFileIO::GetFileSize(CONST String& path) CONST
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) return 0;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fclose(f);
	return (UInt32)size;
}

MYRENDERER_END_NAMESPACE  // Emscripten
MYRENDERER_END_NAMESPACE  // Platform
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
