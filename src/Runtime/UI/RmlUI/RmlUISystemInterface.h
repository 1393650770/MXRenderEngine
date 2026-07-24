
#pragma once
#ifndef _RMLUISYSTEMINTERFACE_
#define _RMLUISYSTEMINTERFACE_

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(RmlUI)

/**
 * Rml::SystemInterface implementation.
 *
 * Bridges RmlUI framework services to the engine's platform layer:
 * - Time (via RmlUISystem::Update accumulated time, replaces glfwGetTime)
 * - Logging (via std::cout / MXRender log macros)
 * - Clipboard (optional, v1: no-op)
 * - Mouse cursor (optional, v1: no-op)
 */
MYRENDERER_BEGIN_CLASS(RmlUISystemInterface)

#pragma region METHOD
public:
	RmlUISystemInterface();
	VIRTUAL ~RmlUISystemInterface();

	/// Set the current elapsed time. Called by RmlUISystem::Update(dt).
	void METHOD(SetElapsedTime)(Float64 time);

	/// Install this as the active SystemInterface for RmlUI.
	/// Must be called before Rml::Initialise().
	void METHOD(Install)();

	/// Uninstall this SystemInterface. Called during shutdown.
	void METHOD(Uninstall)();

protected:
private:
	// Rml::SystemInterface virtuals are implemented via internal class
	// to avoid exposing RmlUi headers in this header.
	class Impl;
	Impl* m_impl = nullptr;
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // !_RMLUISYSTEMINTERFACE_
