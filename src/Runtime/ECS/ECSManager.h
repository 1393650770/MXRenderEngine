#pragma once
#ifndef _ECS_MANAGER_
#define _ECS_MANAGER_

// ECSManager — singleton facade for the ECS subsystem (Pattern C).
//
// Holds an ECSSystem* backend (injected via Create()), provides type-safe
// template APIs that forward to the type-erased ECSSystem raw methods.
//
// Usage (application layer):
//   ECSManager::Create(new Ennt::EnntECSSystem());
//   auto entity = ECSManager::Get().CreateEntity();
//   auto* comp = ECSManager::Get().AddComponent<TransformComponent>(entity);
//   ECSManager::Get().DestroyEntity(entity);
//   ECSManager::Destroy();
//
// Zero EnTT dependency — the concrete backend is opaque to the application layer.

#include "Core/ConstDefine.h"
#include "ECS/ECSSystem.h"
#include "ECS/ComponentTypeID.h"
#include "ECS/Ennt/EnntECSSystem.h"  // for dynamic_cast in RegisterComponent

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(ECS)

MYRENDERER_BEGIN_CLASS(ECSManager)
#pragma region METHOD
public:
	static void METHOD(Create)(ECSSystem* backend);
	static ECSManager& METHOD(Get)();
	static void METHOD(Destroy)();

	ECSSystem* METHOD(GetBackend)() CONST { return m_backend; }

	// ---- Entity lifecycle ----
	EntityHandle METHOD(CreateEntity)()
	{
		return m_backend ? m_backend->CreateEntity() : EntityHandle{};
	}

	void METHOD(DestroyEntity)(EntityHandle entity)
	{
		if (m_backend) m_backend->DestroyEntity(entity);
	}

	Bool METHOD(IsValid)(EntityHandle entity) CONST
	{
		return m_backend ? m_backend->IsValid(entity) : false;
	}

	// ---- Component management (type-safe templates) ----
	// Pre-register a component type (call once before AddComponent<T>)
	template<typename T>
	void METHOD(RegisterComponent)()
	{
		if (!m_backend) return;
		auto* ennt = dynamic_cast<MXRender::ECS::Ennt::EnntECSSystem*>(m_backend);
		if (ennt) ennt->RegisterComponentType<T>();
	}

	template<typename T>
	T* METHOD(AddComponent)(EntityHandle entity)
	{
		if (!m_backend) return nullptr;
		UInt32 type_id = ComponentTypeID::Get<T>();
		void* raw = m_backend->AddComponentRaw(entity, type_id, sizeof(T));
		return STATIC_CAST(raw, T);
	}

	template<typename T>
	T* METHOD(GetComponent)(EntityHandle entity) CONST
	{
		if (!m_backend) return nullptr;
		UInt32 type_id = ComponentTypeID::Get<T>();
		void* raw = m_backend->GetComponentRaw(entity, type_id);
		return STATIC_CAST(raw, T);
	}

	template<typename T>
	Bool METHOD(HasComponent)(EntityHandle entity) CONST
	{
		if (!m_backend) return false;
		return m_backend->HasComponentRaw(entity, ComponentTypeID::Get<T>());
	}

	template<typename T>
	void METHOD(RemoveComponent)(EntityHandle entity)
	{
		if (m_backend) m_backend->RemoveComponentRaw(entity, ComponentTypeID::Get<T>());
	}

	template<typename... Components>
	void METHOD(ForEach)(std::function<void(EntityHandle)> callback)
	{
		if (!m_backend) return;
		Vector<UInt32> type_ids = { ComponentTypeID::Get<Components>()... };
		m_backend->ForEach(type_ids, std::move(callback));
	}

	// ---- Per-frame ----
	void METHOD(Update)(Float32 dt)
	{
		if (m_backend) m_backend->Update(dt);
	}

	// ---- Garbage collect ----
	void METHOD(GarbageCollect)()
	{
		if (m_backend) m_backend->GarbageCollect();
	}
protected:
private:
#pragma endregion

#pragma region MEMBER
private:
	ECSSystem* m_backend = nullptr;
	static ECSManager* s_instance;

	ECSManager() MYDEFAULT;
	~ECSManager() MYDEFAULT;
	ECSManager(CONST ECSManager&) MYDELETE;
	ECSManager& operator=(CONST ECSManager&) MYDELETE;
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // ECS
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _ECS_MANAGER_
