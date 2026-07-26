#pragma once
#ifndef _RPC_SERVICE_
#define _RPC_SERVICE_

// RPCService — abstract base for services that expose remote procedure calls.
// Each concrete service (e.g. PlayerService, ChatService) inherits this
// and overrides RegisterHandlers() to wire RPC IDs to method implementations.
//
// Usage:
//   class PlayerService : public RPCService {
//       void RegisterHandlers(RPCDispatcher* d) override {
//           d->RegisterHandler(1, [this](auto* p, auto len, auto sid, auto reply) {
//               // deserialize & dispatch to OnFire
//           });
//       }
//       void OnFire(Vector3 target);
//   };
//
// Generated code (Phase 4) produces RegisterHandlers() automatically from
// RPC(method, target) annotations.

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Network)
MYRENDERER_BEGIN_NAMESPACE(RPC)

class RPCDispatcher;  // fwd

MYRENDERER_BEGIN_CLASS(RPCService)
#pragma region METHOD
public:
	RPCService() MYDEFAULT;
	VIRTUAL ~RPCService() MYDEFAULT;

	// Called once at startup to register all RPC handlers.
	// Generated code overrides this.
	VIRTUAL void METHOD(RegisterHandlers)(RPCDispatcher* dispatcher) {}

	// Optional: called after all handlers are registered
	VIRTUAL void METHOD(OnServiceReady)() {}

	// Optional: per-frame tick for this service
	VIRTUAL void METHOD(Tick)(Float32 dt) {}
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // RPC
MYRENDERER_END_NAMESPACE  // Network
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _RPC_SERVICE_
