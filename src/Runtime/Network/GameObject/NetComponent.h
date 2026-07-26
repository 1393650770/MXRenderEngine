#pragma once
#ifndef _NET_COMPONENT_
#define _NET_COMPONENT_

// NetComponent — base class for network-replicated ECS components.
//
// Inherits from ReplicationObject (Phase 2) to participate in the
// replication system, and stores an ECS::EntityHandle back-reference
// to link network operations to ECS entities.
//
// Usage (with code generation, Phase 4):
//   struct HealthComponent : public NetComponent {
//       REPLICATE(hp, OnChange)
//       Float32 hp = 100.0f;
//   };
//   auto entity = ECSManager::Get().CreateEntity();
//   auto* comp = ECSManager::Get().AddComponent<HealthComponent>(entity);
//   // comp is now registered in both ECS and Replication systems

#include "Core/ConstDefine.h"
#include "Network/Replication/ReplicationObject.h"
#include "ECS/ECSSystem.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(GameObject)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(NetComponent, public Replication::ReplicationObject)
#pragma region METHOD
public:
	NetComponent() MYDEFAULT;
	VIRTUAL ~NetComponent() OVERRIDE;

	ECS::EntityHandle METHOD(GetEntity)() CONST { return m_entity; }
	void METHOD(SetEntity)(ECS::EntityHandle e) { m_entity = e; }
protected:
private:
#pragma endregion

#pragma region MEMBER
protected:
	ECS::EntityHandle m_entity;
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // GameObject
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _NET_COMPONENT_
