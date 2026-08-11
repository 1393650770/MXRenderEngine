#include "ConversionReport.h"

#include <cstdio>
#include <string>

namespace MXRender::Tool::CssToRcss {

void ConversionReport::PrintToConsole() const
{
	for (const auto& issue : m_issues)
	{
		if (issue.selector.empty())
			std::printf("[CssToRcss] line %d: %s\n", issue.line, issue.message.c_str());
		else
			std::printf("[CssToRcss] line %d: [%s] %s\n", issue.line, issue.selector.c_str(), issue.message.c_str());
	}
}

std::string ConversionReport::ToJson() const
{
	std::string out = "{\n  \"issues\": [\n";
	for (size_t i = 0; i < m_issues.size(); ++i)
	{
		const auto& issue = m_issues[i];
		std::string selector_json = issue.selector;
		for (size_t p = 0; p < selector_json.size(); ++p)
			if (selector_json[p] == '"' || selector_json[p] == '\\')
				selector_json.insert(p++, 1, '\\');
		std::string message_json = issue.message;
		for (size_t p = 0; p < message_json.size(); ++p)
			if (message_json[p] == '"' || message_json[p] == '\\')
				message_json.insert(p++, 1, '\\');

		out += "    { \"line\": " + std::to_string(issue.line)
			+ ", \"selector\": \"" + selector_json + "\""
			+ ", \"message\": \"" + message_json + "\" }";
		if (i + 1 < m_issues.size()) out += ",";
		out += "\n";
	}
	out += "  ]\n}\n";
	return out;
}

} // namespace MXRender::Tool::CssToRcss
