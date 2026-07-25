#pragma once
#ifndef _NETWORK_MANAGER_
#define _NETWORK_MANAGER_

// Network Manager — Singleton Facade (patterned after UIManager).
// Holds NetworkSystem* backend, injected via Create().

#include "Core/ConstDefine.h"
#include "Network/NetworkSystem.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)

MYRENDERER_BEGIN_CLASS(NetworkManager)
#pragma region METHOD
public:
	static void METHOD(Create)(NetworkSystem* backend);
	static NetworkManager& METHOD(Get)();
	static void METHOD(Destroy)();

	NetworkSystem* METHOD(GetBackend)() CONST { return m_backend; }

	// Convenience delegates (null-safe)
	void METHOD(Update)() { if (m_backend) m_backend->Update(); }
	void METHOD(HTTPGet)(const String& url, HTTPCallback cb) { if (m_backend) m_backend->HTTPGet(url, cb); }
	void METHOD(HTTPPost)(const String& url, const String& body, HTTPCallback cb) { if (m_backend) m_backend->HTTPPost(url, body, cb); }

protected:
private:
	NetworkManager() MYDEFAULT;
	static NetworkManager* s_instance;
	NetworkSystem* m_backend = nullptr;
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _NETWORK_MANAGER_
