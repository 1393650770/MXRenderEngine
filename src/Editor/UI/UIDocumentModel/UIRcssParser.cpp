#include "UIRcssParser.h"
#include "CssParser.h"     // CssToRcssLib
#include "RcssEmitter.h"   // shared JoinTokens (bracket/colon/paren-aware)

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(UIDocModel)

namespace {

using namespace MXRender::Tool::CssToRcss;

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
	std::vector<CssToken> part;
	for (const auto& t : rule.selector)
	{
		if (t.kind == ETokenKind::Comma)
		{
			String sel = JoinTokens(part);
			if (!sel.empty()) out.selectors.push_back(std::move(sel));
			part.clear();
			continue;
		}
		part.push_back(t);
	}
	String last = JoinTokens(part);
	if (!last.empty()) out.selectors.push_back(std::move(last));
	for (const auto& d : rule.declarations)
		out.properties.push_back(UICssProperty{ d.property, JoinTokens(d.value), d.line });
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
		uiat.prelude = JoinTokens(at.prelude);
		for (const auto& rule : at.rules)
			uiat.rules.push_back(ToRuleSet(rule));
		for (const auto& d : at.declarations)
			uiat.properties.push_back(UICssProperty{ d.property, JoinTokens(d.value), d.line });
		out.at_rules.push_back(std::move(uiat));
	}
	for (const auto& issue : css.issues)
		out.errors.push_back(UIError{ issue.line, issue.message });
	return out;
}

MYRENDERER_END_NAMESPACE // UIDocModel
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
