#pragma once
#ifndef _COMPONENT_TYPE_ID_
#define _COMPONENT_TYPE_ID_

// ComponentTypeID — per-component-type unique identifier (no RTTI needed).
//
// Each component type T gets a unique UInt32 at first call to Get<T>().
// The ID is stable for the lifetime of the process and serves as the key
// for type-erased component operations in ECSSystem.
//
// Mirrors the concept of entt::type_info but in our abstract layer,
// keeping EnTT out of all headers except EnntECSSystem.cpp.

#include "Core/ConstDefine.h"
#include <atomic>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(ECS)

MYRENDERER_BEGIN_CLASS(ComponentTypeID)
#pragma region METHOD
public:
	template<typename T>
	static UInt32 METHOD(Get)()
	{
		static UInt32 id = s_next_id.fetch_add(1, std::memory_order_relaxed);
		return id;
	}

	static UInt32 METHOD(GetNextID)() { return s_next_id.load(std::memory_order_relaxed); }
protected:
private:
#pragma endregion

#pragma region MEMBER
private:
	static inline std::atomic<UInt32> s_next_id{1};
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // ECS
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _COMPONENT_TYPE_ID_
