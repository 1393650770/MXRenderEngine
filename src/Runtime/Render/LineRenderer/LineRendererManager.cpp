#include "Render/LineRenderer/LineRendererManager.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)

LineRendererManager* LineRendererManager::s_instance = nullptr;

void LineRendererManager::Create()
{
	if (s_instance)
		return;  // idempotent
	s_instance = new LineRendererManager();
}

void LineRendererManager::Destroy()
{
	delete s_instance;
	s_instance = nullptr;
}

LineRendererManager& LineRendererManager::Get()
{
	// Matches UIManager-style Get: caller must Create() first (shutdown order:
	// world dies before Manager::Destroy, so no collector runs after).
	return *s_instance;
}

MYRENDERER_END_NAMESPACE  // Render
MYRENDERER_END_NAMESPACE  // MXRender
