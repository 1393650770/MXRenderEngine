#pragma once
#ifndef _PLATFORM_ASYNC_FILEIO_
#define _PLATFORM_ASYNC_FILEIO_

// Async File I/O Abstraction — Bridge pattern.
// Callback-based (not std::future) for wasm single-threaded safety.
// Browser: fetch() via EM_ASM, Desktop: std::async + fread.

#include "Core/ConstDefine.h"
#include <functional>
#include <vector>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Platform)

using FileIOCallback = std::function<void(const String& path, Vector<UInt8> data, Bool success)>;

MYRENDERER_BEGIN_CLASS(PlatformAsyncFileIO)
#pragma region METHOD
public:
	PlatformAsyncFileIO() MYDEFAULT;
	VIRTUAL ~PlatformAsyncFileIO() MYDEFAULT;

	// Initiate async read. Callback fires on main thread when complete.
	VIRTUAL void METHOD(ReadFileAsync)(const String& path, FileIOCallback callback) {}

	// Cancel a pending read (no-op if already completed)
	VIRTUAL void METHOD(CancelPending)(const String& path) {}

	// Check if a read is still in-flight
	VIRTUAL Bool METHOD(IsPending)(const String& path) CONST { return false; }

protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Platform
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _PLATFORM_ASYNC_FILEIO_
