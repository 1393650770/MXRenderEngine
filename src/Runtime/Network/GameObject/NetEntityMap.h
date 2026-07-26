#pragma once
#ifndef _NET_ENTITY_MAP_
#define _NET_ENTITY_MAP_

// NetEntityMap — bidirectional map between ECS::EntityHandle and NetObjectID.
//
// Bridges the ECS world (entities addressed by generation-protected handles)
// and the network replication world (objects addressed by NetObjectID).
//
// Singleton. Used by ReplicationManager to find which ECS entity corresponds
// to an incoming network ID, and by NetSpawnSystem to register new entities.

#include "Core/ConstDefine.h"
#include "ECS/ECSSystem.h"
#include "Network/Replication/ReplicationObject.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(GameObject)

MYRENDERER_BEGIN_CLASS(NetEntityMap)
#pragma region METHOD
public:
	static NetEntityMap& METHOD(Get)();

	void METHOD(Map)(ECS::EntityHandle entity, Replication::NetObjectID net_id);
	void METHOD(Unmap)(ECS::EntityHandle entity);
	void METHOD(UnmapByNetID)(Replication::NetObjectID net_id);

	Replication::NetObjectID METHOD(GetNetID)(ECS::EntityHandle entity) CONST;
	ECS::EntityHandle METHOD(GetEntity)(Replication::NetObjectID net_id) CONST;

	Bool METHOD(IsMapped)(ECS::EntityHandle entity) CONST;
	Bool METHOD(IsMappedByNetID)(Replication::NetObjectID net_id) CONST;

	UInt32 METHOD(GetCount)() CONST { return (UInt32)m_entity_to_net.size(); }
protected:
private:
#pragma endregion

#pragma region MEMBER
private:
	Map<ECS::EntityHandle, Replication::NetObjectID> m_entity_to_net;
	Map<Replication::NetObjectID, ECS::EntityHandle> m_net_to_entity;
#pragma endregion
MYRENDERER_END_CLASS

// ---- Inline impl ----

inline NetEntityMap& NetEntityMap::Get()
{
	static NetEntityMap instance;
	return instance;
}

inline void NetEntityMap::Map(ECS::EntityHandle entity, Replication::NetObjectID net_id)
{
	m_entity_to_net[entity] = net_id;
	m_net_to_entity[net_id] = entity;
}

inline void NetEntityMap::Unmap(ECS::EntityHandle entity)
{
	auto it = m_entity_to_net.find(entity);
	if (it != m_entity_to_net.end())
	{
		m_net_to_entity.erase(it->second);
		m_entity_to_net.erase(it);
	}
}

inline void NetEntityMap::UnmapByNetID(Replication::NetObjectID net_id)
{
	auto it = m_net_to_entity.find(net_id);
	if (it != m_net_to_entity.end())
	{
		m_entity_to_net.erase(it->second);
		m_net_to_entity.erase(it);
	}
}

inline Replication::NetObjectID NetEntityMap::GetNetID(ECS::EntityHandle entity) CONST
{
	auto it = m_entity_to_net.find(entity);
	return (it != m_entity_to_net.end()) ? it->second : Replication::kInvalidNetID;
}

inline ECS::EntityHandle NetEntityMap::GetEntity(Replication::NetObjectID net_id) CONST
{
	auto it = m_net_to_entity.find(net_id);
	return (it != m_net_to_entity.end()) ? it->second : ECS::EntityHandle{};
}

inline Bool NetEntityMap::IsMapped(ECS::EntityHandle entity) CONST
{
	return m_entity_to_net.find(entity) != m_entity_to_net.end();
}

inline Bool NetEntityMap::IsMappedByNetID(Replication::NetObjectID net_id) CONST
{
	return m_net_to_entity.find(net_id) != m_net_to_entity.end();
}

MYRENDERER_END_NAMESPACE  // GameObject
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _NET_ENTITY_MAP_
