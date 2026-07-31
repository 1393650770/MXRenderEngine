#include "World/GameplayEventBus.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

void GameplayEventBus::ClearAll()
{
	for (auto& entry : channels_)
	{
		if (entry.second)
			entry.second->Clear();
	}
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender