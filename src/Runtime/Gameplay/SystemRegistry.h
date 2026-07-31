#pragma once
#ifndef _SYSTEM_REGISTRY_
#define _SYSTEM_REGISTRY_

#include "Core/ConstDefine.h"
#include "Gameplay/ISystem.h"
#include <utility>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Gameplay)

// Registry of gameplay systems. Registration order == execution order
// (documented contract). Name is debug metadata only.
MYRENDERER_BEGIN_CLASS(SystemRegistry)
#pragma region METHOD
public:
	SystemRegistry() MYDEFAULT;
	~SystemRegistry() MYDEFAULT;

	void METHOD(Register)(String debug_name, UniquePtr<ISystem> system);
	void METHOD(RunAll)(World::GameWorld& world, Float32 dt);
	void METHOD(Clear)();

protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	Vector<std::pair<String, UniquePtr<ISystem>>> systems_;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Gameplay
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _SYSTEM_REGISTRY_