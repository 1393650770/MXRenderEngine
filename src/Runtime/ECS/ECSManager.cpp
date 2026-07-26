#include "ECS/ECSManager.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(ECS)

MXRender::ECS::ECSManager* ECSManager::s_instance = nullptr;

void ECSManager::Create(ECSSystem* backend)
{
	if (s_instance || !backend) return;
	s_instance = new ECSManager();
	s_instance->m_backend = backend;
}

ECSManager& ECSManager::Get()
{
	static ECSManager fallback;
	return s_instance ? *s_instance : fallback;
}

void ECSManager::Destroy()
{
	if (!s_instance) return;
	delete s_instance->m_backend;
	s_instance->m_backend = nullptr;
	delete s_instance;
	s_instance = nullptr;
}

MYRENDERER_END_NAMESPACE  // ECS
MYRENDERER_END_NAMESPACE  // MXRender
