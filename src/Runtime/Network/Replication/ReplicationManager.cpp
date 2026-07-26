#include "Network/Replication/ReplicationManager.h"
#include "Network/NetworkManager.h"
#include "Network/BinaryBuffer.h"
#include <cstring>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(Replication)

MXRender::Network::Replication::ReplicationManager* ReplicationManager::s_instance = nullptr;

void ReplicationManager::Create(NetworkManager* net)
{
	if (s_instance || !net) return;
	s_instance = new ReplicationManager();
	s_instance->m_network = net;
}

void ReplicationManager::Shutdown()
{
	if (!s_instance) return;
	s_instance->m_objects.clear();
	s_instance->m_previous_states.clear();
	delete s_instance;
	s_instance = nullptr;
}

ReplicationManager& ReplicationManager::Get()
{
	static ReplicationManager fallback;
	return s_instance ? *s_instance : fallback;
}

void ReplicationManager::Update()
{
	if (m_is_server)
	{
		BroadcastState();
	}
}

void ReplicationManager::RegisterObject(NetObjectID id, ReplicationObject* obj)
{
	if (!obj || id == kInvalidNetID) return;
	m_objects[id] = obj;
	obj->SetNetID(id);
}

void ReplicationManager::UnregisterObject(NetObjectID id)
{
	m_objects.erase(id);
	m_previous_states.erase(id);
}

ReplicationObject* ReplicationManager::FindObject(NetObjectID id) CONST
{
	auto it = m_objects.find(id);
	return (it != m_objects.end()) ? it->second : nullptr;
}

void ReplicationManager::BroadcastState()
{
	if (!m_network) return;

	for (auto& pair : m_objects)
	{
		NetObjectID id = pair.first;
		ReplicationObject* obj = pair.second;
		if (!obj) continue;

		PropertyMask mask = obj->GetDirtyProperties();
		if (mask == 0) continue;

		// Serialize dirty properties
		UInt8 buffer[2048];
		UInt32 len = 0;
		obj->Serialize(buffer, len, sizeof(buffer), mask);

		if (len == 0) continue;

		// Build packet: [net_id:4][property_mask:4][data:len]
		UInt32 packet_size = 4 + 4 + len;
		Vector<UInt8> packet(packet_size);
		*(UInt32*)(packet.data()) = id;
		*(UInt32*)(packet.data() + 4) = mask;
		memcpy(packet.data() + 8, buffer, len);

		// Send via transport
		m_network->WebSocketSend(String(reinterpret_cast<Char*>(packet.data()), packet_size));

		// Store current state for delta compression next tick
		Vector<UInt8> full_state(len);
		memcpy(full_state.data(), buffer, len);
		m_previous_states[id] = std::move(full_state);

		obj->ClearDirtyProperties();
	}
}

void ReplicationManager::ApplySnapshot(NetObjectID obj_id, CONST UInt8* data,
	UInt32 len, PropertyMask mask)
{
	ReplicationObject* obj = FindObject(obj_id);
	if (!obj) return;

	obj->Deserialize(data, len, mask);
}

MYRENDERER_END_NAMESPACE  // Replication
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender
