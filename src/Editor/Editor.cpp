#include <iostream>
#include <cstdlib>
#include "Application/Window.h"
#include "EditorRender/Render.h"

int main()
{
	// Single-thread mode: the editor's ImGui chain (NewFrame → record → replay
	// of ImGui_ImplVulkan_RenderDrawData) is not safe across the 3-thread
	// pipeline — ImGui's Vulkan backend is not thread-safe and its draw-data
	// buffers are reused on the next NewFrame. Single mode runs everything
	// inline on one thread (the render command queue bypasses), which is also
	// exactly the sample UI lifecycle. The editor is a tool: latency is fine.
	_putenv_s("MX_THREAD_MODE", "single");

	MXRender::Application::Window window("MXRender - Render View");
	MXRender::Application::EditorRenderPipeline render(window.GetPlatformWindow());
	window.InitWindow();
	window.Run(&render);
	system("pause");

	return 0;
}
