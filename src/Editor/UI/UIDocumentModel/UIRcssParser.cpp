#include "UIRcssParser.h"
#include "CssParser.h"   // CssToRcssLib

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(UIDocModel)

namespace {

using namespace MXRender::Tool::CssToRcss;

String JoinSelector(const std::vector<CssToken>& selector)
{
	String out;
	for (const auto& t : selector)
	{
		if (t.kind == ETokenKind::Function) { out += t.text; out += '('; continue; }
		if (t.kind == ETokenKind::RParen) { out += ')'; continue; }
		if (!out.empty() && t.kind != ETokenKind::Comma) out += ' ';
		out += t.text;
	}
	return out;
}

String RenderValue(const std::vector<CssToken>& value)
{
	String out;
	for (const auto& t : value)
	{
		if (!out.empty()) out += ' ';
		if (t.kind == ETokenKind::Function) { out += t.text; out += '('; }
		else { out += t.text; }
	}
	return out;
}

EAtRuleKind MapAtRuleKind(const String& name)
{
	if (name == "media") return EAtRuleKind::Media;
	if (name == "keyframes") return EAtRuleKind::Keyframes;
	if (name == "font-face") return EAtRuleKind::FontFace;
	if (name == "decorator") return EAtRuleKind::Decorator;
	if (name == "spritesheet") return EAtRuleKind::SpriteSheet;
	return EAtRuleKind::Unknown;
}

UIRuleSet ToRuleSet(const CssRule& rule)
{
	UIRuleSet out;
	out.source_line = rule.line;
	String current;
	for (const auto& t : rule.selector)
	{
		if (t.kind == ETokenKind::Comma)
		{
			String trimmed = current;
			size_t b = trimmed.find_first_not_of(" \t\r\n");
			size_t e = trimmed.find_last_not_of(" \t\r\n");
			trimmed = (b == String::npos) ? "" : trimmed.substr(b, e - b + 1);
			if (!trimmed.empty()) out.selectors.push_back(trimmed);
			current.clear();
			continue;
		}
		if (!current.empty()) current += ' ';
		current += t.text;
	}
	{
		String trimmed = current;
		size_t b = trimmed.find_first_not_of(" \t\r\n");
		size_t e = trimmed.find_last_not_of(" \t\r\n");
		trimmed = (b == String::npos) ? "" : trimmed.substr(b, e - b + 1);
		if (!trimmed.empty()) out.selectors.push_back(trimmed);
	}
	for (const auto& d : rule.declarations)
		out.properties.push_back(UICssProperty{ d.property, RenderValue(d.value), d.line });
	return out;
}

} // namespace

UIStyleSheet UIRcssParser::Parse(const String& text, const String& file_path)
{
	UIStyleSheet out;
	out.file_path = file_path;

	CssStylesheet css = CssParser::Parse(text);
	for (const auto& rule : css.rules)
		out.rules.push_back(ToRuleSet(rule));
	for (const auto& at : css.at_rules)
	{
		if (at.kind == MXRender::Tool::CssToRcss::EAtRuleKind::Unknown) continue;
		UIAtRule uiat;
		uiat.kind = MapAtRuleKind(at.name);
		uiat.name = at.name;
		uiat.source_line = at.line;
		for (const auto& t : at.prelude)
			uiat.prelude += (uiat.prelude.empty() ? "" : " ") + t.text;
		for (const auto& rule : at.rules)
			uiat.rules.push_back(ToRuleSet(rule));
		for (const auto& d : at.declarations)
			uiat.properties.push_back(UICssProperty{ d.property, RenderValue(d.value), d.line });
		out.at_rules.push_back(std::move(uiat));
	}
	for (const auto& issue : css.issues)
		out.errors.push_back(UIError{ issue.line, issue.message });
	return out;
}

MYRENDERER_END_NAMESPACE // UIDocModel
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
