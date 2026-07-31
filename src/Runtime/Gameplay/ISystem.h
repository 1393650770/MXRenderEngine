#pragma once
#ifndef _GAMEPLAY_ISYSTEM_
#define _GAMEPLAY_ISYSTEM_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)
class GameWorld;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Gameplay)

// Template Method: a gameplay system implements Run; GameWorld orders them
// via SystemRegistry (registration order == execution order).
MYRENDERER_BEGIN_CLASS(ISystem)
#pragma region METHOD
public:
	VIRTUAL ~ISystem() MYDEFAULT;
	VIRTUAL void METHOD(Run)(World::GameWorld& world, Float32 dt) PURE;
protected:

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Gameplay
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _GAMEPLAY_ISYSTEM_