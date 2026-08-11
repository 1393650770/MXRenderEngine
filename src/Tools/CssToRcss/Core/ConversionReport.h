#pragma once
#ifndef _CONVERSION_REPORT_
#define _CONVERSION_REPORT_

#include "CssAst.h"
#include <string>

namespace MXRender::Tool::CssToRcss {

/// Prints and optionally JSON-serializes conversion issues.
/// Console format: [CssToRcss] line:message (selector)
class ConversionReport
{
public:
	explicit ConversionReport(const CssStylesheet& stylesheet) : m_issues(stylesheet.issues) {}

	void PrintToConsole() const;
	/// Hand-rolled minimal JSON writer (keeps the tool dependency-free).
	std::string ToJson() const;

private:
	const std::vector<CssIssue>& m_issues;
};

} // namespace MXRender::Tool::CssToRcss

#endif // !_CONVERSION_REPORT_
