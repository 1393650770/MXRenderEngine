#pragma once
#ifndef _NETWORK_SYSTEM_
#define _NETWORK_SYSTEM_

// Network System Abstraction — Strategy pattern (like AudioSystem/RenderRHI).
// Virtual methods with empty defaults — platforms override what they need.
// Browser: fetch/WebSocket via EM_ASM, Desktop: BSD socket + KCP.

#include "Core/ConstDefine.h"
#include <functional>
#include <memory>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)

using HTTPCallback = std::function<void(Bool success, const String& response)>;
using WebSocketCallback = std::function<void(const String& message)>;

MYRENDERER_BEGIN_CLASS(NetworkSystem)
#pragma region METHOD
public:
	NetworkSystem() MYDEFAULT;
	VIRTUAL ~NetworkSystem() MYDEFAULT;

	// HTTP
	VIRTUAL void METHOD(HTTPGet)(const String& url, HTTPCallback cb) {}
	VIRTUAL void METHOD(HTTPPost)(const String& url, const String& body, HTTPCallback cb) {}

	// WebSocket
	VIRTUAL void METHOD(WebSocketConnect)(const String& url, WebSocketCallback on_msg) {}
	VIRTUAL void METHOD(WebSocketSend)(const String& msg) {}
	VIRTUAL void METHOD(WebSocketClose)() {}

	// Called each frame to pump async callbacks
	VIRTUAL void METHOD(Update)() {}

protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _NETWORK_SYSTEM_
