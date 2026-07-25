#if PLATFORM_GLES3

#include "Platform/Platform.h"
#include "RHI/GLES3/GLES3_RenderRHI.h"

MXRender::RHI::RenderRHI* PlatformCreateDynamicRHI()
{
	MXRender::RHI::GLES3::GLES3_RenderRHI* pRHI = new MXRender::RHI::GLES3::GLES3_RenderRHI();
	MXRender::RHI::RenderFactory factory;
	factory.threading_mode = EThreadingMode::Single;
	pRHI->Init(&factory);
	return pRHI;
}

#endif // PLATFORM_GLES3
