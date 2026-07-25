#pragma once
#ifndef _BROWSER_ASYNC_FILEIO_
#define _BROWSER_ASYNC_FILEIO_

#if PLATFORM_GLES3

// Browser Async File I/O — fetch() via EM_ASM with main-thread callback polling.

#include "Platform/PlatformAsyncFileIO.h"
#include <emscripten.h>
#include <map>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Platform)
MYRENDERER_BEGIN_NAMESPACE(Emscripten)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(BrowserAsyncFileIO, public PlatformAsyncFileIO)
#pragma region METHOD
public:
	BrowserAsyncFileIO() MYDEFAULT;
	VIRTUAL ~BrowserAsyncFileIO() MYDEFAULT;

	VIRTUAL void METHOD(ReadFileAsync)(const String& path, FileIOCallback callback) OVERRIDE FINAL;
	VIRTUAL void METHOD(CancelPending)(const String& path) OVERRIDE FINAL;
	VIRTUAL Bool METHOD(IsPending)(const String& path) CONST OVERRIDE FINAL;

	// Call each frame to deliver completed callbacks
	void METHOD(Update)();
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	struct PendingRead { FileIOCallback callback; };
	std::map<String, PendingRead> m_pending;
private:
#pragma endregion
MYRENDERER_END_CLASS

// ---- Inline impl ----

inline void BrowserAsyncFileIO::ReadFileAsync(const String& path, FileIOCallback callback)
{
	if (!callback) return;
	m_pending[path] = { callback };

	// Use fetch() to load the file asynchronously.
	// When done, store data + trigger delivery in Update().
	EM_ASM({
		var path = UTF8ToString($0);
		console.log('[BrowserAsyncFileIO] Fetching:', path);
		fetch(path).then(function(response) {
			if (!response.ok) throw new Error('HTTP ' + response.status);
			return response.arrayBuffer();
		}).then(function(buffer) {
			var bytes = new Uint8Array(buffer);
			// Allocate Emscripten heap memory and copy
			var ptr = _malloc(bytes.length);
			HEAPU8.set(bytes, ptr);
			// Store completion data for polling
			if (!Module._async_io_results) Module._async_io_results = {};
			Module._async_io_results[path] = { ptr: ptr, size: bytes.length, success: 1 };
		}).catch(function(err) {
			console.log('[BrowserAsyncFileIO] Failed:', path, err);
			if (!Module._async_io_results) Module._async_io_results = {};
			Module._async_io_results[path] = { ptr: 0, size: 0, success: 0 };
		});
	}, path.c_str());
}

inline void BrowserAsyncFileIO::CancelPending(const String& path)
{
	m_pending.erase(path);
	EM_ASM({
		if (Module._async_io_results) delete Module._async_io_results[UTF8ToString($0)];
	}, path.c_str());
}

inline Bool BrowserAsyncFileIO::IsPending(const String& path) CONST
{
	return m_pending.find(path) != m_pending.end();
}

inline void BrowserAsyncFileIO::Update()
{
	// Poll JS-side results and deliver callbacks
	for (auto it = m_pending.begin(); it != m_pending.end(); )
	{
		const String& path = it->first;
		Bool completed = false;
		Bool success = false;
		Vector<UInt8> data;

		// Check if JS has stored a result
		Int has_result = EM_ASM_INT({
			var path = UTF8ToString($0);
			var results = Module._async_io_results;
			return (results && results[path]) ? 1 : 0;
		}, path.c_str());

		if (has_result)
		{
			// Retrieve result from JS heap
			Int ptr = EM_ASM_INT({
				var path = UTF8ToString($0);
				var r = Module._async_io_results[path];
				return r ? r.ptr : 0;
			}, path.c_str());
			Int size = EM_ASM_INT({
				var path = UTF8ToString($0);
				var r = Module._async_io_results[path];
				return r ? r.size : 0;
			}, path.c_str());
			Int ok = EM_ASM_INT({
				var path = UTF8ToString($0);
				var r = Module._async_io_results[path];
				var s = r ? r.success : 0;
				delete Module._async_io_results[path];
				return s;
			}, path.c_str());

			if (ok && ptr && size > 0)
			{
				data.resize(size);
				memcpy(data.data(), (const void*)(uintptr_t)ptr, size);
				::free((void*)(uintptr_t)ptr);  // free Emscripten heap allocation
				success = true;
			}
			completed = true;
		}

		if (completed)
		{
			auto callback = it->second.callback;
			it = m_pending.erase(it);
			if (callback) callback(path, data, success);
		}
		else
		{
			++it;
		}
	}
}

MYRENDERER_END_NAMESPACE  // Emscripten
MYRENDERER_END_NAMESPACE  // Platform
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _BROWSER_ASYNC_FILEIO_
