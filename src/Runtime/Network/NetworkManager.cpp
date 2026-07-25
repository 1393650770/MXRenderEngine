#include "Network/NetworkManager.h"

MXRender::Network::NetworkManager* MXRender::Network::NetworkManager::s_instance = nullptr;

void MXRender::Network::NetworkManager::Create(NetworkSystem* backend)
{
	if (s_instance || !backend) return;
	s_instance = new NetworkManager();
	s_instance->m_backend = backend;
}

MXRender::Network::NetworkManager& MXRender::Network::NetworkManager::Get()
{
	static NetworkManager fallback;  // safe default if Create() not called
	return s_instance ? *s_instance : fallback;
}

void MXRender::Network::NetworkManager::Destroy()
{
	if (!s_instance) return;
	delete s_instance;
	s_instance = nullptr;
}
