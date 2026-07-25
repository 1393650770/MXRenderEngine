#pragma once
#ifndef _BROWSER_NETWORK_
#define _BROWSER_NETWORK_

#if PLATFORM_GLES3

// Browser Network — fetch() + WebSocket via EM_ASM.
// Implements NetworkSystem for the browser/WASM platform.

#include "Network/NetworkSystem.h"
#include <emscripten.h>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(Emscripten)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(BrowserNetwork, public NetworkSystem)
#pragma region METHOD
public:
	BrowserNetwork() MYDEFAULT;
	VIRTUAL ~BrowserNetwork() MYDEFAULT;

	VIRTUAL void METHOD(HTTPGet)(const String& url, HTTPCallback cb) OVERRIDE FINAL;
	VIRTUAL void METHOD(HTTPPost)(const String& url, const String& body, HTTPCallback cb) OVERRIDE FINAL;
	VIRTUAL void METHOD(WebSocketConnect)(const String& url, WebSocketCallback on_msg) OVERRIDE FINAL;
	VIRTUAL void METHOD(WebSocketSend)(const String& msg) OVERRIDE FINAL;
	VIRTUAL void METHOD(WebSocketClose)() OVERRIDE FINAL;
	VIRTUAL void METHOD(Update)() OVERRIDE FINAL;
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

// ---- Inline impl ----

inline void BrowserNetwork::HTTPGet(const String& url, HTTPCallback cb)
{
	EM_ASM({
		var url = UTF8ToString($0);
		fetch(url).then(function(r) { return r.text(); }).then(function(t) {
			console.log('[BrowserNetwork] GET', url, t.length, 'bytes');
		}).catch(function(e) { console.log('[BrowserNetwork] GET failed:', e); });
	}, url.c_str());
	if (cb) cb(true, "");
}

inline void BrowserNetwork::HTTPPost(const String& url, const String& body, HTTPCallback cb)
{
	String js_body = body;  // pass through
	EM_ASM({
		var url = UTF8ToString($0); var body = UTF8ToString($1);
		fetch(url, {method:'POST', body:body}).then(function(r){return r.text()}).then(function(t){
			console.log('[BrowserNetwork] POST', url, t.length, 'bytes');
		}).catch(function(e){console.log('[BrowserNetwork] POST failed:',e);});
	}, url.c_str(), js_body.c_str());
	if (cb) cb(true, "");
}

inline void BrowserNetwork::WebSocketConnect(const String& url, WebSocketCallback on_msg)
{
	EM_ASM({
		var url = UTF8ToString($0);
		try {
			var ws = new WebSocket(url);
			ws.onmessage = function(e) { console.log('[BrowserNetwork] WS msg:', e.data); };
			ws.onopen = function() { console.log('[BrowserNetwork] WS connected:', url); };
			Module._browser_ws = ws;
		} catch(e) { console.log('[BrowserNetwork] WS failed:', e); }
	}, url.c_str());
}

inline void BrowserNetwork::WebSocketSend(const String& msg)
{
	EM_ASM({
		if (Module._browser_ws) Module._browser_ws.send(UTF8ToString($0));
	}, msg.c_str());
}

inline void BrowserNetwork::WebSocketClose()
{
	EM_ASM({ if (Module._browser_ws) { Module._browser_ws.close(); Module._browser_ws = null; } });
}

inline void BrowserNetwork::Update()
{
	// Async callbacks are handled by the browser event loop — no polling needed
}

MYRENDERER_END_NAMESPACE  // Emscripten
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
#endif // _BROWSER_NETWORK_
