#pragma once
#ifndef _ENNT_ECS_SYSTEM_
#define _ENNT_ECS_SYSTEM_

// EnntECSSystem — EnTT-based concrete ECSSystem.
// ONLY file to #include <entt/entt.hpp>.

#include "ECS/ECSSystem.h"
#include "ECS/ComponentTypeID.h"
#include "Core/ResourceRegistry.h"
#include <entt/entt.hpp>
#include <tuple>
#include <utility>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(ECS)
MYRENDERER_BEGIN_NAMESPACE(Ennt)

class EntityEntry
{
public:
	entt::entity id = entt::null;
};

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(EnntECSSystem, public ECSSystem)
#pragma region METHOD
public:
	EnntECSSystem();
	VIRTUAL ~EnntECSSystem() OVERRIDE;

	VIRTUAL EntityHandle METHOD(CreateEntity)() OVERRIDE FINAL;
	VIRTUAL void METHOD(DestroyEntity)(EntityHandle entity) OVERRIDE FINAL;
	VIRTUAL Bool METHOD(IsValid)(EntityHandle entity) CONST OVERRIDE FINAL;

	VIRTUAL void* METHOD(AddComponentRaw)(EntityHandle entity, UInt32 type_id, UInt32 size) OVERRIDE FINAL;
	VIRTUAL void  METHOD(RemoveComponentRaw)(EntityHandle entity, UInt32 type_id) OVERRIDE FINAL;
	VIRTUAL void* METHOD(GetComponentRaw)(EntityHandle entity, UInt32 type_id) CONST OVERRIDE FINAL;
	VIRTUAL Bool  METHOD(HasComponentRaw)(EntityHandle entity, UInt32 type_id) CONST OVERRIDE FINAL;

	VIRTUAL void METHOD(ForEach)(CONST Vector<UInt32>& type_ids,
		std::function<void(EntityHandle)> callback) OVERRIDE FINAL;

	VIRTUAL void METHOD(GarbageCollect)() OVERRIDE FINAL;

	entt::registry& METHOD(GetRegistry)() { return m_registry; }

	template<typename T>
	void METHOD(RegisterComponentType)();

	// Typed parallel iteration over the primary storage (First) sliced by
	// index - zero materialization, cache-affine partitions. Rest components
	// are filtered per entity (view semantics). Callable from JobSystem
	// workers: NO structural changes may happen inside (EnTT registry is not
	// thread-safe for create/destroy/emplace/remove - see JobContext guards).
	template<typename First, typename... Rest>
	void ParallelForEach(UInt32 partition_index, UInt32 partition_count,
		CONST std::function<void(First&, Rest&...)>& callback);

protected:
	struct ComponentOps
	{
		std::function<void*(entt::registry&, entt::entity)> add;
		std::function<void(entt::registry&, entt::entity)>   remove;
		std::function<void*(const entt::registry&, entt::entity)> get;
		std::function<bool(const entt::registry&, entt::entity)>  has;
	};

	void* AddComponentTyped(UInt32 type_id, entt::entity e);
	void  RemoveComponentTyped(UInt32 type_id, entt::entity e);
	void* GetComponentTyped(UInt32 type_id, entt::entity e) CONST;
	Bool  HasComponentTyped(UInt32 type_id, entt::entity e) CONST;
	EntityHandle FindHandle(entt::entity e) CONST;
private:
#pragma endregion

#pragma region MEMBER
private:
	entt::registry m_registry;
	MXRender::ResourceRegistry<EntityEntry> m_entity_registry;
	Map<UInt32, ComponentOps> m_component_ops;
	Vector<entt::entity> m_deferred_destroy;
	Map<entt::entity, UInt32>  m_entity_to_handle;
#pragma endregion
MYRENDERER_END_CLASS

// ============================================================
// Template impl
// ============================================================

template<typename T>
inline void EnntECSSystem::RegisterComponentType()
{
	UInt32 type_id = ComponentTypeID::Get<T>();
	if (m_component_ops.find(type_id) != m_component_ops.end()) return;
	ComponentOps ops;
	ops.add = [](entt::registry& reg, entt::entity e) -> void* {
		return &reg.emplace<T>(e);
	};
	ops.remove = [](entt::registry& reg, entt::entity e) {
		reg.remove<T>(e);
	};
	ops.get = [](const entt::registry& reg, entt::entity e) -> void* {
		return const_cast<T*>(reg.try_get<T>(e));
	};
	ops.has = [](const entt::registry& reg, entt::entity e) -> bool {
		return reg.all_of<T>(e);
	};
	m_component_ops[type_id] = std::move(ops);
}

inline void* EnntECSSystem::AddComponentTyped(UInt32 type_id, entt::entity e)
{
	auto it = m_component_ops.find(type_id);
	return (it != m_component_ops.end()) ? it->second.add(m_registry, e) : nullptr;
}
inline void EnntECSSystem::RemoveComponentTyped(UInt32 type_id, entt::entity e)
{
	auto it = m_component_ops.find(type_id);
	if (it != m_component_ops.end()) it->second.remove(m_registry, e);
}
inline void* EnntECSSystem::GetComponentTyped(UInt32 type_id, entt::entity e) CONST
{
	auto it = m_component_ops.find(type_id);
	return (it != m_component_ops.end()) ? it->second.get(m_registry, e) : nullptr;
}
inline Bool EnntECSSystem::HasComponentTyped(UInt32 type_id, entt::entity e) CONST
{
	auto it = m_component_ops.find(type_id);
	return (it != m_component_ops.end()) ? it->second.has(m_registry, e) : false;
}
inline EntityHandle EnntECSSystem::FindHandle(entt::entity e) CONST
{
	auto it = m_entity_to_handle.find(e);
	if (it == m_entity_to_handle.end()) return {};
	EntityHandle h;
	h.value = it->second;
	return h;
}

// ---- Inline main impl ----

inline EnntECSSystem::EnntECSSystem() {}
inline EnntECSSystem::~EnntECSSystem()
{
	GarbageCollect();
	m_registry.clear();
	m_entity_to_handle.clear();
}

inline EntityHandle EnntECSSystem::CreateEntity()
{
	entt::entity e = m_registry.create();
	auto* entry = new EntityEntry{ e };
	GenericHandle raw_h = m_entity_registry.Allocate(entry, "Entity");
	EntityHandle handle;
	handle.value = raw_h;
	m_entity_to_handle[e] = raw_h;
	return handle;
}

inline void EnntECSSystem::DestroyEntity(EntityHandle entity)
{
	auto* entry = m_entity_registry.Resolve(entity.value);
	if (!entry) return;
	m_entity_to_handle.erase(entry->id);
	m_deferred_destroy.push_back(entry->id);
	delete entry;
	m_entity_registry.Free(entity.value);
}

inline Bool EnntECSSystem::IsValid(EntityHandle entity) CONST
{
	auto* entry = m_entity_registry.Resolve(entity.value);
	return entry && m_registry.valid(entry->id);
}

inline void* EnntECSSystem::AddComponentRaw(EntityHandle entity, UInt32 type_id, UInt32 size)
{
	auto* entry = m_entity_registry.Resolve(entity.value);
	if (!entry) return nullptr;
	if (m_component_ops.find(type_id) == m_component_ops.end()) return nullptr;
	return AddComponentTyped(type_id, entry->id);
}

inline void EnntECSSystem::RemoveComponentRaw(EntityHandle entity, UInt32 type_id)
{
	auto* entry = m_entity_registry.Resolve(entity.value);
	if (!entry) return;
	RemoveComponentTyped(type_id, entry->id);
}

inline void* EnntECSSystem::GetComponentRaw(EntityHandle entity, UInt32 type_id) CONST
{
	auto* entry = m_entity_registry.Resolve(entity.value);
	if (!entry) return nullptr;
	return GetComponentTyped(type_id, entry->id);
}

inline Bool EnntECSSystem::HasComponentRaw(EntityHandle entity, UInt32 type_id) CONST
{
	auto* entry = m_entity_registry.Resolve(entity.value);
	if (!entry) return false;
	return HasComponentTyped(type_id, entry->id);
}

inline void EnntECSSystem::ForEach(CONST Vector<UInt32>& type_ids,
	std::function<void(EntityHandle)> callback)
{
	if (type_ids.empty() || !callback) return;

	auto view = m_registry.view<entt::entity>();
	for (auto e : view)
	{
		Bool has_all = true;
		for (UInt32 tid : type_ids)
			if (!HasComponentTyped(tid, e)) { has_all = false; break; }
		if (has_all)
		{
			EntityHandle handle = FindHandle(e);
			if (handle.IsValid()) callback(handle);
		}
	}
}

inline void EnntECSSystem::GarbageCollect()
{
	for (entt::entity e : m_deferred_destroy)
		if (m_registry.valid(e)) m_registry.destroy(e);
	m_deferred_destroy.clear();
}

template<typename First, typename... Rest>
inline void EnntECSSystem::ParallelForEach(UInt32 partition_index, UInt32 partition_count,
	CONST std::function<void(First&, Rest&...)>& callback)
{
	if (partition_count == 0 || !callback)
		return;

	// Primary storage: packed entity array + component array are index-aligned
	// (EnTT 3.13 basic_storage). Slice the entity array.
	//
	// IMPORTANT: cache storage references OUTSIDE the loop. EnTT's registry
	// accessors (assure -> dense_map lookup) are ~23us/call in this build -
	// per-entity registry.get/all_of would make the loop ~100x slower. Direct
	// storage access is the whole point of the typed path.
	auto& main_storage = m_registry.storage<First>();
	const std::size_t total = main_storage.size();
	if (total == 0)
		return;

	const std::size_t begin = total * partition_index / partition_count;
	const std::size_t end = total * (partition_index + 1) / partition_count;
	const entt::entity* entities = main_storage.data();

	if constexpr (sizeof...(Rest) == 0)
	{
		for (std::size_t i = begin; i < end; ++i)
			callback(main_storage.get(entities[i]));
		return;
	}

	// Rest storages cached once per partition; per-entity access is a direct
	// sparse lookup (no registry round-trip).
	auto rest_storages = std::make_tuple(&m_registry.storage<std::remove_const_t<Rest>>()...);
	std::apply([&](auto*... rs) {
		for (std::size_t i = begin; i < end; ++i)
		{
			const entt::entity e = entities[i];
			if (!(rs->contains(e) && ...))
				continue;  // view semantics: entity must have every component
			callback(main_storage.get(e), rs->get(e)...);
		}
	}, rest_storages);
}

MYRENDERER_END_NAMESPACE  // Ennt
MYRENDERER_END_NAMESPACE  // ECS
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _ENNT_ECS_SYSTEM_
