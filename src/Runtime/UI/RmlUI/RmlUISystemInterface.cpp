#include "RmlUISystemInterface.h"

#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/Log.h>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(RmlUI)

// =========================================================================
// Internal Impl class that actually derives from Rml::SystemInterface
// =========================================================================
class RmlUISystemInterface::Impl : public Rml::SystemInterface
{
public:
	Float64 elapsed_time = 0.0;

	// Log capture state (hot-reload tooling). All RmlUi activity runs on the
	// render thread, so this needs no locking.
	bool capturing_log = false;
	Int   log_warning_count = 0;

	double GetElapsedTime() override
	{
		return elapsed_time;
	}

	bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
	{
		// Only hard errors abort a hot-reload swap: RmlUi reports recoverable
		// content issues (missing data-model variable, unresolved font face)
		// as WARNING, and the tolerant XML parser still yields a document for
		// those — aborting on them would reject perfectly loadable documents.
		if (capturing_log && type == Rml::Log::LT_ERROR)
			++log_warning_count;

		// Route RmlUI logs to std::cout (matches project convention)
		const char* prefix = "";
		switch (type)
		{
		case Rml::Log::LT_ERROR:   prefix = "[RmlUI Error] "; break;
		case Rml::Log::LT_WARNING: prefix = "[RmlUI Warn]  "; break;
		case Rml::Log::LT_INFO:    prefix = "[RmlUI Info]  "; break;
		case Rml::Log::LT_DEBUG:   prefix = "[RmlUI Debug] "; break;
		default:                   prefix = "[RmlUI]       "; break;
		}
		std::cout << prefix << message << std::endl;
		return true;
	}
};

// =========================================================================
// RmlUISystemInterface
// =========================================================================
RmlUISystemInterface::RmlUISystemInterface()
{
	m_impl = new Impl();
}

RmlUISystemInterface::~RmlUISystemInterface()
{
	Uninstall();
	delete m_impl;
	m_impl = nullptr;
}

void RmlUISystemInterface::SetElapsedTime(Float64 time)
{
	if (m_impl)
		m_impl->elapsed_time = time;
}

void RmlUISystemInterface::Install()
{
	if (m_impl)
	{
		Rml::SetSystemInterface(m_impl);
	}
}

void RmlUISystemInterface::Uninstall()
{
	if (m_impl)
	{
		Rml::SetSystemInterface(nullptr);
	}
}

void RmlUISystemInterface::BeginLogCapture()
{
	if (!m_impl) return;
	m_impl->log_warning_count = 0;
	m_impl->capturing_log = true;
}

Int RmlUISystemInterface::EndLogCapture()
{
	if (!m_impl) return 0;
	m_impl->capturing_log = false;
	return m_impl->log_warning_count;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
