#if PLATFORM_WGPU
#include "Platform/Platform.h"
#include "RHI/WebGPU/WGPU_RenderRHI.h"

MXRender::RHI::RenderRHI* PlatformCreateDynamicRHI()
{
	MXRender::RHI::WebGPU::WGPU_RenderRHI* pRHI = new MXRender::RHI::WebGPU::WGPU_RenderRHI();
	MXRender::RHI::RenderFactory factory;
	factory.threading_mode = MXRender::EThreadingMode::Single;
	pRHI->Init(&factory);
	return pRHI;
}
#endif
