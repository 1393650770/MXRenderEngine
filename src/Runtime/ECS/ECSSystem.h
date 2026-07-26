#pragma once
#ifndef _ECS_SYSTEM_
#define _ECS_SYSTEM_

// ECSSystem — abstract base for Entity-Component-System backends.
//
// Pattern A (virtual empty defaults, like UISystem/NetworkSystem).
// EnntECSSystem is the concrete EnTT implementation.
// ECSManager is the singleton facade (Pattern C).
//
// The abstract interface uses type-erased component operations
// (AddComponentRaw/GetComponentRaw/etc.) with ComponentTypeID as key.
// ECSManager provides type-safe template wrappers on top.
//
// EntityHandle uses the global ResourceHandle<TagEntity> identity pattern.
//
// Layer rules: ECSSystem must NOT include EnTT, RHI, or Vulkan headers.

#include "Core/ConstDefine.h"
#include "Core/ResourceHandle.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(ECS)

// Entity identity tag (global handle system)
struct TagEntity {};
using EntityHandle = ResourceHandle<TagEntity>;

MYRENDERER_BEGIN_CLASS(ECSSystem)
#pragma region METHOD
public:
	ECSSystem() MYDEFAULT;
	VIRTUAL ~ECSSystem() MYDEFAULT;

	// ---- Entity lifecycle ----
	VIRTUAL EntityHandle METHOD(CreateEntity)() { return {}; }
	VIRTUAL void METHOD(DestroyEntity)(EntityHandle entity) {}
	VIRTUAL Bool METHOD(IsValid)(EntityHandle entity) CONST { return false; }

	// ---- Component management (type-erased, called by ECSManager templates) ----
	VIRTUAL void* METHOD(AddComponentRaw)(EntityHandle entity, UInt32 type_id, UInt32 size) { return nullptr; }
	VIRTUAL void  METHOD(RemoveComponentRaw)(EntityHandle entity, UInt32 type_id) {}
	VIRTUAL void* METHOD(GetComponentRaw)(EntityHandle entity, UInt32 type_id) CONST { return nullptr; }
	VIRTUAL Bool  METHOD(HasComponentRaw)(EntityHandle entity, UInt32 type_id) CONST { return false; }

	// ---- View / Query (type-erased batch iteration) ----
	VIRTUAL void METHOD(ForEach)(CONST Vector<UInt32>& component_type_ids,
		std::function<void(EntityHandle)> callback) {}

	// ---- Per-frame ----
	VIRTUAL void METHOD(Update)(Float32 dt) {}

	// ---- Garbage collect (deferred destruction) ----
	VIRTUAL void METHOD(GarbageCollect)() {}
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // ECS
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _ECS_SYSTEM_
