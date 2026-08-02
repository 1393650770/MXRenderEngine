#include "RmlUIInputBridge.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Input.h>
#include "Input/InputSystem.h"
#include "Render/Core/CommandQueue.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(RmlUI)

void RmlUIInputBridge::SetContext(Rml::Context* context)
{
	m_context = context;
}

// All Process* methods enqueue onto the render thread: Rml::Context is
// single-threaded and Update()/Render() run there. Bypass mode (no render
// thread) executes inline - identical semantics. The captured context pointer
// stays valid: the command queue drains before UIManager::Destroy.
void RmlUIInputBridge::ProcessMouseMove(Int x, Int y)
{
	if (!m_context) return;
	ENQUEUE_RENDER_COMMAND(RmlMouseMove)([ctx = m_context, x, y, mods = BuildModifiers()]()
	{
		ctx->ProcessMouseMove(x, y, mods);
	});
}

void RmlUIInputBridge::ProcessMouseButton(Int button, bool pressed)
{
	if (!m_context) return;
	ENQUEUE_RENDER_COMMAND(RmlMouseButton)([ctx = m_context, button, pressed, mods = BuildModifiers()]()
	{
		if (pressed)
			ctx->ProcessMouseButtonDown(button, mods);
		else
			ctx->ProcessMouseButtonUp(button, mods);
	});
}

void RmlUIInputBridge::ProcessMouseScroll(Float32 dx, Float32 dy)
{
	if (!m_context) return;
	ENQUEUE_RENDER_COMMAND(RmlMouseScroll)([ctx = m_context, dx, dy, mods = BuildModifiers()]()
	{
		ctx->ProcessMouseWheel({ dx, dy }, mods);
	});
}

void RmlUIInputBridge::ProcessKey(CONST MXRender::Input::Key& key, bool pressed)
{
	if (!m_context) return;
	ENQUEUE_RENDER_COMMAND(RmlKey)([ctx = m_context, rml_key = static_cast<Rml::Input::KeyIdentifier>(MapKey(key)), pressed, mods = BuildModifiers()]()
	{
		if (pressed)
			ctx->ProcessKeyDown(rml_key, mods);
		else
			ctx->ProcessKeyUp(rml_key, mods);
	});
}

void RmlUIInputBridge::ProcessChar(UInt32 codepoint)
{
	if (!m_context) return;
	ENQUEUE_RENDER_COMMAND(RmlChar)([ctx = m_context, codepoint]()
	{
		ctx->ProcessTextInput(static_cast<Rml::Character>(codepoint));
	});
}

void RmlUIInputBridge::ProcessMouseLeave()
{
	if (!m_context) return;
	ENQUEUE_RENDER_COMMAND(RmlMouseLeave)([ctx = m_context]()
	{
		ctx->ProcessMouseLeave();
	});
}

bool RmlUIInputBridge::IsMouseInteracting() CONST
{
	if (!m_context) return false;
	return m_context->IsMouseInteracting();
}

// =========================================================================
// BuildModifiers — uses InputSystem (no GLFW dependency)
// =========================================================================
Int RmlUIInputBridge::BuildModifiers() CONST
{
	using namespace MXRender::Input;
	auto& input = InputSystem::Get();
	int mods = 0;

	if (input.IsKeyDown(EKey::LeftCtrl) || input.IsKeyDown(EKey::RightCtrl))
		mods |= Rml::Input::KM_CTRL;
	if (input.IsKeyDown(EKey::LeftShift) || input.IsKeyDown(EKey::RightShift))
		mods |= Rml::Input::KM_SHIFT;
	if (input.IsKeyDown(EKey::LeftAlt) || input.IsKeyDown(EKey::RightAlt))
		mods |= Rml::Input::KM_ALT;

	return mods;
}

// =========================================================================
// MapKey — EKey → Rml::Input::KeyIdentifier (platform-neutral mapping)
// =========================================================================
Int RmlUIInputBridge::MapKey(CONST MXRender::Input::Key& key)
{
	using namespace MXRender::Input;
	using KI = Rml::Input::KeyIdentifier;

	if      (key == EKey::Space)      return static_cast<Int>(KI::KI_SPACE);
	if      (key == EKey::K0)         return static_cast<Int>(KI::KI_0);
	if      (key == EKey::K1)         return static_cast<Int>(KI::KI_1);
	if      (key == EKey::K2)         return static_cast<Int>(KI::KI_2);
	if      (key == EKey::K3)         return static_cast<Int>(KI::KI_3);
	if      (key == EKey::K4)         return static_cast<Int>(KI::KI_4);
	if      (key == EKey::K5)         return static_cast<Int>(KI::KI_5);
	if      (key == EKey::K6)         return static_cast<Int>(KI::KI_6);
	if      (key == EKey::K7)         return static_cast<Int>(KI::KI_7);
	if      (key == EKey::K8)         return static_cast<Int>(KI::KI_8);
	if      (key == EKey::K9)         return static_cast<Int>(KI::KI_9);
	if      (key == EKey::A)          return static_cast<Int>(KI::KI_A);
	if      (key == EKey::B)          return static_cast<Int>(KI::KI_B);
	if      (key == EKey::C)          return static_cast<Int>(KI::KI_C);
	if      (key == EKey::D)          return static_cast<Int>(KI::KI_D);
	if      (key == EKey::E)          return static_cast<Int>(KI::KI_E);
	if      (key == EKey::F)          return static_cast<Int>(KI::KI_F);
	if      (key == EKey::G)          return static_cast<Int>(KI::KI_G);
	if      (key == EKey::H)          return static_cast<Int>(KI::KI_H);
	if      (key == EKey::I)          return static_cast<Int>(KI::KI_I);
	if      (key == EKey::J)          return static_cast<Int>(KI::KI_J);
	if      (key == EKey::K)          return static_cast<Int>(KI::KI_K);
	if      (key == EKey::L)          return static_cast<Int>(KI::KI_L);
	if      (key == EKey::M)          return static_cast<Int>(KI::KI_M);
	if      (key == EKey::N)          return static_cast<Int>(KI::KI_N);
	if      (key == EKey::O)          return static_cast<Int>(KI::KI_O);
	if      (key == EKey::P)          return static_cast<Int>(KI::KI_P);
	if      (key == EKey::Q)          return static_cast<Int>(KI::KI_Q);
	if      (key == EKey::R)          return static_cast<Int>(KI::KI_R);
	if      (key == EKey::S)          return static_cast<Int>(KI::KI_S);
	if      (key == EKey::T)          return static_cast<Int>(KI::KI_T);
	if      (key == EKey::U)          return static_cast<Int>(KI::KI_U);
	if      (key == EKey::V)          return static_cast<Int>(KI::KI_V);
	if      (key == EKey::W)          return static_cast<Int>(KI::KI_W);
	if      (key == EKey::X)          return static_cast<Int>(KI::KI_X);
	if      (key == EKey::Y)          return static_cast<Int>(KI::KI_Y);
	if      (key == EKey::Z)          return static_cast<Int>(KI::KI_Z);
	if      (key == EKey::Escape)     return static_cast<Int>(KI::KI_ESCAPE);
	if      (key == EKey::Tab)        return static_cast<Int>(KI::KI_TAB);
	if      (key == EKey::Backspace)  return static_cast<Int>(KI::KI_BACK);
	if      (key == EKey::Enter)      return static_cast<Int>(KI::KI_RETURN);
	if      (key == EKey::Left)       return static_cast<Int>(KI::KI_LEFT);
	if      (key == EKey::Right)      return static_cast<Int>(KI::KI_RIGHT);
	if      (key == EKey::Up)         return static_cast<Int>(KI::KI_UP);
	if      (key == EKey::Down)       return static_cast<Int>(KI::KI_DOWN);
	if      (key == EKey::Delete)     return static_cast<Int>(KI::KI_DELETE);
	if      (key == EKey::F1)         return static_cast<Int>(KI::KI_F1);
	if      (key == EKey::F2)         return static_cast<Int>(KI::KI_F2);
	if      (key == EKey::F3)         return static_cast<Int>(KI::KI_F3);
	if      (key == EKey::F4)         return static_cast<Int>(KI::KI_F4);
	if      (key == EKey::F5)         return static_cast<Int>(KI::KI_F5);
	if      (key == EKey::F6)         return static_cast<Int>(KI::KI_F6);
	if      (key == EKey::F7)         return static_cast<Int>(KI::KI_F7);
	if      (key == EKey::F8)         return static_cast<Int>(KI::KI_F8);
	if      (key == EKey::F9)         return static_cast<Int>(KI::KI_F9);
	if      (key == EKey::F10)        return static_cast<Int>(KI::KI_F10);
	if      (key == EKey::F11)        return static_cast<Int>(KI::KI_F11);
	if      (key == EKey::F12)        return static_cast<Int>(KI::KI_F12);

	return static_cast<Int>(KI::KI_UNKNOWN);
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
