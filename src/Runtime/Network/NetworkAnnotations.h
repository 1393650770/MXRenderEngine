#pragma once
#ifndef _NETWORK_ANNOTATIONS_
#define _NETWORK_ANNOTATIONS_

// Network replication & RPC annotation macros.
//
// GCC/Clang __attribute__((annotate(...))) syntax — each macro places
// the attribute BEFORE the declaration it describes.
//
// In non-parser builds, all macros expand to nothing (zero-cost, valid C++).
//
// Usage:
//   struct NET_CLASS(1) PlayerState : public NetGameObject {
//       REPLICATE(OnChange) Float32 health = 100.0f;
//       REPLICATE(Always)  Vector3  position;
//       OWNER_ONLY         Float32  secret_value = 0.0f;
//       RPC(Server) void OnFire(Vector3 target);
//       RPC(Multicast) void OnRespawn();
//   };
//
// Supported conditions: "OnChange" (dirty-tracking), "Always" (every tick)
// Supported RPC targets: "Server", "Client", "Multicast"

#include "Core/ConstDefine.h"

#if defined(__REFLECTION_PARSER__)

// Mark a field for network replication with a condition
#define REPLICATE(condition)    __attribute__((annotate("REPLICATE:" #condition)))

// Mark a method as a remote procedure call
#define RPC(target)             __attribute__((annotate("RPC:" #target)))

// Assign a class ID for the spawn system (place BEFORE struct/class keyword)
#define NET_CLASS(id)           __attribute__((annotate("NETCLASS:" #id)))

// Mark a field as visible only to the owning client
#define OWNER_ONLY              __attribute__((annotate("OWNERONLY")))

// Mark an ECS component struct as replicable (place BEFORE struct keyword)
#define REPLICATED_COMPONENT    __attribute__((annotate("REPLICATED_COMPONENT")))

#else  // !__REFLECTION_PARSER__

#define REPLICATE(condition)
#define RPC(target)
#define NET_CLASS(id)
#define OWNER_ONLY
#define REPLICATED_COMPONENT

#endif // __REFLECTION_PARSER__

#endif // _NETWORK_ANNOTATIONS_
