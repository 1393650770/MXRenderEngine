#if PLATFORM_WIN32
#include "Platform/Platform.h"
#include "RHI/Vulkan/VK_RenderRHI.h"

MXRender::RHI::RenderRHI* PlatformCreateDynamicRHI()
{
	MXRender::RHI::Vulkan::VulkanRHI* pRHI = new MXRender::RHI::Vulkan::VulkanRHI();
	MXRender::RHI::RenderFactory factory;
	factory.enable_render_debug = false;
	factory.validation_level = 0;
	factory.enable_debug_callback = false;
	factory.threading_mode = EThreadingMode::ThreeThread;
	pRHI->Init(&factory);

	return pRHI;
}
#endif
