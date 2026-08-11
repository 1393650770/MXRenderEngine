#include "DesktopWindow.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "Input/InputSystem.h"
#include "Input/InputKeys.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)

DesktopWindow* DesktopWindow::s_instance = nullptr;

// GLFW → InputSystem callbacks
static void GlfwKeyCallback(GLFWwindow*, Int key, Int, Int action, Int)
{
	auto& is = MXRender::Input::InputSystem::Get();
	if (action == GLFW_PRESS || action == GLFW_REPEAT)
		is.FeedKeyDown(key);
	else if (action == GLFW_RELEASE)
		is.FeedKeyUp(key);
}
static void GlfwMouseButtonCallback(GLFWwindow*, Int btn, Int action, Int)
{
	MXRender::Input::InputSystem::Get().FeedMouseButton(btn, action == GLFW_PRESS);
}
static void GlfwCursorPosCallback(GLFWwindow*, double x, double y)
{
	MXRender::Input::InputSystem::Get().FeedMousePos((Float32)x, (Float32)y);
}
static void GlfwScrollCallback(GLFWwindow*, double, double y)
{
	MXRender::Input::InputSystem::Get().FeedScroll((Float32)y);
}
static void GlfwCharCallback(GLFWwindow*, unsigned int codepoint)
{
	// UTF-32 codepoint — matches Rml::Character / UIInputBridge::ProcessChar
	MXRender::Input::InputSystem::Get().FeedChar((UInt32)codepoint);
}

DesktopWindow::DesktopWindow(const String& title, UInt32 w, UInt32 h)
{
	s_instance = this;
	MXRender::Input::PlatformKeyMap::RegisterDesktop();
	if (!glfwInit())
		return;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	m_glfw_window = glfwCreateWindow((int)w, (int)h, title.c_str(), nullptr, nullptr);
	if (!m_glfw_window)
	{
		glfwTerminate();
		return;
	}
	glfwSetKeyCallback(m_glfw_window, GlfwKeyCallback);
	glfwSetMouseButtonCallback(m_glfw_window, GlfwMouseButtonCallback);
	glfwSetCursorPosCallback(m_glfw_window, GlfwCursorPosCallback);
	glfwSetScrollCallback(m_glfw_window, GlfwScrollCallback);
	glfwSetCharCallback(m_glfw_window, GlfwCharCallback);
}

DesktopWindow::~DesktopWindow()
{
	if (m_glfw_window) { glfwDestroyWindow(m_glfw_window); m_glfw_window = nullptr; }
	glfwTerminate();
	if (s_instance == this) s_instance = nullptr;
}

void DesktopWindow::GetFramebufferSize(Int& w, Int& h) CONST
{
	if (m_glfw_window) glfwGetFramebufferSize(m_glfw_window, &w, &h);
}

void DesktopWindow::PollEvents()
{
	MXRender::Input::InputSystem::Get().BeginFrame();
	glfwPollEvents();
	glfwSwapBuffers(m_glfw_window); // no-op for Vulkan
}

Bool DesktopWindow::ShouldClose() CONST
{
	return m_glfw_window ? (Bool)glfwWindowShouldClose(m_glfw_window) : true;
}

Float64 DesktopWindow::GetTime() CONST
{
	return (Float64)glfwGetTime();
}

Bool DesktopWindow::GetMouseButton(MouseButton btn) CONST
{
	Int idx = (Int)btn;
	return (idx >= 0 && idx < 3) ? MXRender::Input::InputSystem::Get().IsMouseDown(idx) : false;
}

void DesktopWindow::GetCursorPos(Float64& x, Float64& y) CONST
{
	Float32 fx, fy;
	MXRender::Input::InputSystem::Get().GetMousePos(fx, fy);
	x = (Float64)fx; y = (Float64)fy;
}

void DesktopWindow::SetScrollCallback(void (*cb)(Float64, Float64))
{
	m_scroll_callback = cb;
}

void DesktopWindow::OnScrollStatic(GLFWwindow*, Float64 ox, Float64 oy)
{
	if (s_instance && s_instance->m_scroll_callback)
		s_instance->m_scroll_callback(ox, oy);
}

// Platform factory
UniquePtr<PlatformWindow> CreatePlatformWindow(const String& title, UInt32 w, UInt32 h, void* platform_data)
{
	return std::make_unique<DesktopWindow>(title, w, h);
}

MYRENDERER_END_NAMESPACE
