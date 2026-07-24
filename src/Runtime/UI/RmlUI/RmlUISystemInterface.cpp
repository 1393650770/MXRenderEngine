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

	double GetElapsedTime() override
	{
		return elapsed_time;
	}

	bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
	{
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

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
