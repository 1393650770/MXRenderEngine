#pragma once
#ifndef _NET_SPAWN_SYSTEM_
#define _NET_SPAWN_SYSTEM_

// NetSpawnSystem — manages creation/destruction of networked ECS entities.
//
// Pattern D (factory registry). Maps class_id (UInt16) → factory function
// that creates an ECS entity with all its components.
//
// Server: Spawn() creates the entity locally, assigns a NetObjectID, and
// broadcasts a Spawn message to all clients.
//
// Client: HandleSpawn() receives the message and creates the entity
// using the registered factory, then applies initial state.

#include "Core/ConstDefine.h"
#include "ECS/ECSSystem.h"
#include "Network/Replication/ReplicationObject.h"
#include "Network/Replication/ReplicationManager.h"
#include "Network/GameObject/NetEntityMap.h"
#include "Network/RPC/RPCDispatcher.h"
#include <functional>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(GameObject)

enum class EOwnership : UInt8
{
	ServerOwned,   // Server is authoritative (e.g., NPC, world object)
	ClientOwned,   // One client is authoritative (e.g., player character)
	RemoteOwned,   // Another client is authoritative (rendered with interpolation)
};

using NetEntityFactory = std::function<ECS::EntityHandle()>;

MYRENDERER_BEGIN_CLASS(NetSpawnSystem)
#pragma region METHOD
public:
	static void METHOD(Init)(Replication::ReplicationManager* rep_mgr);
	static NetSpawnSystem& METHOD(Get)();

	// Register a class factory (called by generated code at startup)
	void METHOD(RegisterClass)(UInt16 class_id, NetEntityFactory factory,
		CONST Vector<UInt32>& component_type_ids);

	// Server: spawn a new networked entity and broadcast
	ECS::EntityHandle METHOD(Spawn)(UInt16 class_id, EOwnership ownership);

	// Client: handle an incoming Spawn message
	void METHOD(HandleSpawn)(Replication::NetObjectID net_id, UInt16 class_id,
		CONST UInt8* init_data, UInt32 len);

	// Both: despawn
	void METHOD(Despawn)(Replication::NetObjectID net_id);

	// Query
	ECS::EntityHandle METHOD(GetEntity)(Replication::NetObjectID net_id) CONST;
	UInt32 METHOD(GetSpawnedCount)() CONST { return (UInt32)m_spawned_entities.size(); }

	// ID allocation (server only)
	Replication::NetObjectID METHOD(AllocateNetID)();

protected:
	Replication::ReplicationManager* m_rep_mgr = nullptr;
	UInt32 m_next_net_id = 1;  // server-side ID allocation
private:
#pragma endregion

#pragma region MEMBER
private:
	struct SpawnEntry
	{
		NetEntityFactory   factory;
		Vector<UInt32>     component_types;
	};
	Map<UInt16, SpawnEntry> m_factories;
	Map<Replication::NetObjectID, ECS::EntityHandle> m_spawned_entities;
	static NetSpawnSystem* s_instance;
#pragma endregion
MYRENDERER_END_CLASS

// ---- Inline impl ----

inline void NetSpawnSystem::Init(Replication::ReplicationManager* rep_mgr)
{
	if (!s_instance) s_instance = new NetSpawnSystem();
	s_instance->m_rep_mgr = rep_mgr;
}

inline NetSpawnSystem& NetSpawnSystem::Get()
{
	static NetSpawnSystem fallback;
	return s_instance ? *s_instance : fallback;
}

inline void NetSpawnSystem::RegisterClass(UInt16 class_id, NetEntityFactory factory,
	CONST Vector<UInt32>& component_type_ids)
{
	SpawnEntry entry;
	entry.factory = std::move(factory);
	entry.component_types = component_type_ids;
	m_factories[class_id] = std::move(entry);
}

inline ECS::EntityHandle NetSpawnSystem::Spawn(UInt16 class_id, EOwnership ownership)
{
	auto it = m_factories.find(class_id);
	if (it == m_factories.end()) return {};

	// Create the ECS entity
	ECS::EntityHandle entity = it->second.factory();
	if (!entity.IsValid()) return {};

	// Assign network ID
	Replication::NetObjectID net_id = AllocateNetID();
	NetEntityMap::Get().Map(entity, net_id);

	// Register with replication system (if there's a replicated component)
	if (m_rep_mgr)
	{
		for (UInt32 comp_type : it->second.component_types)
		{
			// Find the ReplicationObject* for this component
			// (This requires the component to implement NetComponent)
			// For now, components are auto-registered by the spawn factory via NetEntityMap
		}
	}

	m_spawned_entities[net_id] = entity;
	return entity;
}

inline void NetSpawnSystem::HandleSpawn(Replication::NetObjectID net_id, UInt16 class_id,
	CONST UInt8* init_data, UInt32 len)
{
	auto it = m_factories.find(class_id);
	if (it == m_factories.end()) return;

	// Create entity on client
	ECS::EntityHandle entity = it->second.factory();
	if (!entity.IsValid()) return;

	// Map the network ID
	NetEntityMap::Get().Map(entity, net_id);
	m_spawned_entities[net_id] = entity;

	// Apply initial state if provided
	if (init_data && len > 0)
	{
		// Deserialize initial component states
		// (Phase 4: generated ReplicationTraits will handle this)
	}
}

inline void NetSpawnSystem::Despawn(Replication::NetObjectID net_id)
{
	auto it = m_spawned_entities.find(net_id);
	if (it == m_spawned_entities.end()) return;

	NetEntityMap::Get().UnmapByNetID(net_id);
	ECS::ECSManager::Get().DestroyEntity(it->second);
	m_spawned_entities.erase(it);
}

inline ECS::EntityHandle NetSpawnSystem::GetEntity(Replication::NetObjectID net_id) CONST
{
	auto it = m_spawned_entities.find(net_id);
	return (it != m_spawned_entities.end()) ? it->second : ECS::EntityHandle{};
}

inline Replication::NetObjectID NetSpawnSystem::AllocateNetID()
{
	return m_next_net_id++;
}

MYRENDERER_END_NAMESPACE  // GameObject
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _NET_SPAWN_SYSTEM_
