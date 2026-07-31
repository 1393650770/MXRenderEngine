#include "Gameplay/SystemRegistry.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Gameplay)

void SystemRegistry::Register(String debug_name, UniquePtr<ISystem> system)
{
	if (system)
		systems_.push_back({ std::move(debug_name), std::move(system) });
}

void SystemRegistry::RunAll(World::GameWorld& world, Float32 dt)
{
	for (auto& entry : systems_)
	{
		if (entry.second)
			entry.second->Run(world, dt);
	}
}

void SystemRegistry::Clear()
{
	systems_.clear();
}

MYRENDERER_END_NAMESPACE  // Gameplay
MYRENDERER_END_NAMESPACE  // MXRender