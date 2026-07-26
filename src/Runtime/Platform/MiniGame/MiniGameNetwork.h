#pragma once
#ifndef _MINIGAME_NETWORK_
#define _MINIGAME_NETWORK_

#if PLATFORM_GLES3

// MiniGame Network — unified WeChat/Douyin mini-game transport.
// Both platforms run in a WebGL/WASM sandbox with a JS bridge.
// Platform API is auto-detected at runtime (wx.* vs tt.*).
//
// WeChat:  wx.connectSocket / wx.sendSocketMessage / wx.onSocketMessage  (wss://)
// Douyin:  tt.connectSocket  / tt.sendSocketMessage  / tt.onSocketMessage  (wss://)
//
// Constraints (both platforms):
//   - Only wss:// allowed, certificates required (TLS 1.2+)
//   - Max 5 concurrent WebSocket connections (WeChat)
//   - Network interrupted 5s after entering background
//   - Domain whitelist required (WeChat)

#include "Network/NetworkSystem.h"
#include <emscripten.h>
#include <vector>
#include <string>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(MiniGame)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(MiniGameNetwork, public NetworkSystem)
#pragma region METHOD
public:
	MiniGameNetwork();
	VIRTUAL ~MiniGameNetwork() OVERRIDE;

	VIRTUAL void METHOD(HTTPGet)(CONST String& url, HTTPCallback cb) OVERRIDE FINAL;
	VIRTUAL void METHOD(HTTPPost)(CONST String& url, CONST String& body, HTTPCallback cb) OVERRIDE FINAL;
	VIRTUAL void METHOD(WebSocketConnect)(CONST String& url, WebSocketCallback on_msg) OVERRIDE FINAL;
	VIRTUAL void METHOD(WebSocketSend)(CONST String& msg) OVERRIDE FINAL;
	VIRTUAL void METHOD(WebSocketClose)() OVERRIDE FINAL;
	VIRTUAL void METHOD(Update)() OVERRIDE FINAL;

	// Query detected platform
	Bool METHOD(IsWeChat)() CONST { return m_is_wx; }
	Bool METHOD(IsDouyin)() CONST { return m_is_tt; }
	Bool METHOD(IsConnected)() CONST { return m_connected; }

protected:
	// Called from JS when a WS message arrives
	static void OnJSMessage(CONST char* data, Int len);
	static void OnJSOpen();
	static void OnJSClose();
	static void OnJSError(CONST char* err);
private:
#pragma endregion

#pragma region MEMBER
private:
	WebSocketCallback m_on_msg;
	Vector<String>    m_recv_queue;    // pending messages from JS callback
	Vector<String>    m_send_queue;    // queued before connection opens
	Bool              m_connected = false;
	Bool              m_is_wx = false;
	Bool              m_is_tt = false;
	static MiniGameNetwork* s_active_instance;  // for static JS callbacks
#pragma endregion
MYRENDERER_END_CLASS

// ---- Inline impl ----

inline MiniGameNetwork::MiniGameNetwork()
{
	s_active_instance = this;

	// Auto-detect platform at construction time
	EM_ASM({
		if (typeof wx !== 'undefined' && typeof wx.connectSocket === 'function') {
			HEAP8[$0] = 1;  // m_is_wx = true
		} else if (typeof tt !== 'undefined' && typeof tt.connectSocket === 'function') {
			HEAP8[$1] = 1;  // m_is_tt = true
		}
	}, &m_is_wx, &m_is_tt);
}

inline MiniGameNetwork::~MiniGameNetwork()
{
	WebSocketClose();
	if (s_active_instance == this) s_active_instance = nullptr;
}

inline void MiniGameNetwork::HTTPGet(CONST String& url, HTTPCallback cb)
{
	EM_ASM({
		var url = UTF8ToString($0);
		var fetchFn = (typeof fetch !== 'undefined') ? fetch :
			(typeof wx !== 'undefined' && wx.request) ? function(u, opts) {
				return new Promise(function(resolve, reject) {
					wx.request({ url: u, method: 'GET', success: function(r) {
						resolve({ text: function() { return Promise.resolve(r.data); } });
					}, fail: reject });
				});
			} : null;

		if (!fetchFn) { console.log('[MiniGameNetwork] No HTTP API available'); return; }
		fetchFn(url).then(function(r) { return r.text(); }).then(function(t) {
			console.log('[MiniGameNetwork] GET', url, t.length, 'bytes');
		}).catch(function(e) { console.log('[MiniGameNetwork] GET failed:', e); });
	}, url.c_str());
	if (cb) cb(true, "");
}

inline void MiniGameNetwork::HTTPPost(CONST String& url, CONST String& body, HTTPCallback cb)
{
	EM_ASM({
		var url = UTF8ToString($0); var body = UTF8ToString($1);
		var fetchFn = (typeof fetch !== 'undefined') ? fetch :
			(typeof wx !== 'undefined' && wx.request) ? function(u, opts) {
				return new Promise(function(resolve, reject) {
					wx.request({ url: u, method: 'POST', data: opts.body, success: function(r) {
						resolve({ text: function() { return Promise.resolve(r.data); } });
					}, fail: reject });
				});
			} : null;

		if (!fetchFn) { console.log('[MiniGameNetwork] No HTTP API available'); return; }
		fetchFn(url, {method:'POST', body:body}).then(function(r){return r.text()}).then(function(t){
			console.log('[MiniGameNetwork] POST', url, t.length, 'bytes');
		}).catch(function(e){console.log('[MiniGameNetwork] POST failed:',e);});
	}, url.c_str(), body.c_str());
	if (cb) cb(true, "");
}

inline void MiniGameNetwork::WebSocketConnect(CONST String& url, WebSocketCallback on_msg)
{
	m_on_msg = on_msg;
	m_connected = false;
	m_send_queue.clear();

	EM_ASM({
		var url = UTF8ToString($0);
		var opts = { url: url, perMessageDeflate: true,
			success: function() { console.log('[MiniGameNetwork] WS connect call succeeded'); },
			fail: function(e)  { console.log('[MiniGameNetwork] WS connect call failed:', e); }
		};

		try {
			if (typeof wx !== 'undefined' && wx.connectSocket) {
				var task = wx.connectSocket(opts);
				wx.onSocketOpen(function() {
					Module._mx_minigame_ws_on_open();
				});
				wx.onSocketMessage(function(res) {
					var len = lengthBytesUTF8(res.data) + 1;
					var buf = Module._malloc(len);
					stringToUTF8(res.data, buf, len);
					Module._mx_minigame_ws_on_message(buf, len - 1);
					Module._free(buf);
				});
				wx.onSocketClose(function() {
					Module._mx_minigame_ws_on_close();
				});
				wx.onSocketError(function(e) {
					var err = 'WeChat WS error';
					var len = lengthBytesUTF8(err) + 1;
					var buf = Module._malloc(len);
					stringToUTF8(err, buf, len);
					Module._mx_minigame_ws_on_error(buf);
					Module._free(buf);
				});
			} else if (typeof tt !== 'undefined' && tt.connectSocket) {
				var task = tt.connectSocket(opts);
				tt.onSocketOpen(function() {
					Module._mx_minigame_ws_on_open();
				});
				tt.onSocketMessage(function(res) {
					var len = lengthBytesUTF8(res.data) + 1;
					var buf = Module._malloc(len);
					stringToUTF8(res.data, buf, len);
					Module._mx_minigame_ws_on_message(buf, len - 1);
					Module._free(buf);
				});
				tt.onSocketClose(function() {
					Module._mx_minigame_ws_on_close();
				});
				tt.onSocketError(function(e) {
					var err = 'Douyin WS error';
					var len = lengthBytesUTF8(err) + 1;
					var buf = Module._malloc(len);
					stringToUTF8(err, buf, len);
					Module._mx_minigame_ws_on_error(buf);
					Module._free(buf);
				});
			} else {
				// Fallback to browser WebSocket (for dev/test in browser)
				var ws = new WebSocket(url);
				ws.onopen = function() { Module._mx_minigame_ws_on_open(); };
				ws.onmessage = function(e) {
					var len = lengthBytesUTF8(e.data) + 1;
					var buf = Module._malloc(len);
					stringToUTF8(e.data, buf, len);
					Module._mx_minigame_ws_on_message(buf, len - 1);
					Module._free(buf);
				};
				ws.onclose = function() { Module._mx_minigame_ws_on_close(); };
				ws.onerror = function(e) {
					var err = 'Browser WS error';
					var len = lengthBytesUTF8(err) + 1;
					var buf = Module._malloc(len);
					stringToUTF8(err, buf, len);
					Module._mx_minigame_ws_on_error(buf);
					Module._free(buf);
				};
				Module._browser_ws = ws;
			}
		} catch(e) { console.log('[MiniGameNetwork] WS connect failed:', e); }
	}, url.c_str());
}

inline void MiniGameNetwork::WebSocketSend(CONST String& msg)
{
	if (!m_connected)
	{
		m_send_queue.push_back(msg);
		return;
	}

	EM_ASM({
		var data = UTF8ToString($0);
		if (typeof wx !== 'undefined' && wx.sendSocketMessage) {
			wx.sendSocketMessage({ data: data });
		} else if (typeof tt !== 'undefined' && tt.sendSocketMessage) {
			tt.sendSocketMessage({ data: data });
		} else if (Module._browser_ws) {
			Module._browser_ws.send(data);
		}
	}, msg.c_str());
}

inline void MiniGameNetwork::WebSocketClose()
{
	m_connected = false;
	EM_ASM({
		if (typeof wx !== 'undefined' && wx.closeSocket) {
			wx.closeSocket();
		} else if (typeof tt !== 'undefined' && tt.closeSocket) {
			tt.closeSocket();
		} else if (Module._browser_ws) {
			Module._browser_ws.close();
			Module._browser_ws = null;
		}
	});
}

inline void MiniGameNetwork::Update()
{
	// Drain messages queued from JS callbacks
	if (m_recv_queue.empty()) return;
	auto queue = std::move(m_recv_queue);
	m_recv_queue.clear();
	for (auto& msg : queue)
	{
		if (m_on_msg) m_on_msg(msg);
	}
}

// Static JS callbacks (registered on Emscripten Module)
inline void MiniGameNetwork::OnJSMessage(CONST char* data, Int len)
{
	if (!s_active_instance) return;
	s_active_instance->m_recv_queue.emplace_back(data, len);
}

inline void MiniGameNetwork::OnJSOpen()
{
	if (!s_active_instance) return;
	s_active_instance->m_connected = true;
	// Flush queued sends
	for (auto& msg : s_active_instance->m_send_queue)
		s_active_instance->WebSocketSend(msg);
	s_active_instance->m_send_queue.clear();
}

inline void MiniGameNetwork::OnJSClose()
{
	if (!s_active_instance) return;
	s_active_instance->m_connected = false;
}

inline void MiniGameNetwork::OnJSError(CONST char* err)
{
	if (!s_active_instance) return;
	s_active_instance->m_connected = false;
}

MYRENDERER_END_NAMESPACE  // MiniGame
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

// Emscripten C-linkage callbacks (called from JS)
extern "C" {
	EMSCRIPTEN_KEEPALIVE
	inline void mx_minigame_ws_on_message(CONST char* data, Int len)
	{
		MXRender::Network::MiniGame::MiniGameNetwork::OnJSMessage(data, len);
	}

	EMSCRIPTEN_KEEPALIVE
	inline void mx_minigame_ws_on_open()
	{
		MXRender::Network::MiniGame::MiniGameNetwork::OnJSOpen();
	}

	EMSCRIPTEN_KEEPALIVE
	inline void mx_minigame_ws_on_close()
	{
		MXRender::Network::MiniGame::MiniGameNetwork::OnJSClose();
	}

	EMSCRIPTEN_KEEPALIVE
	inline void mx_minigame_ws_on_error(CONST char* err)
	{
		MXRender::Network::MiniGame::MiniGameNetwork::OnJSError(err);
	}
}

#endif // PLATFORM_GLES3
#endif // _MINIGAME_NETWORK_
