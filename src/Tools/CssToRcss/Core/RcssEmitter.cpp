#include "RcssEmitter.h"

namespace MXRender::Tool::CssToRcss {

namespace {

/// Joins tokens with single spaces EXCEPT inside function parentheses —
/// `translateX(-50%)` must stay `translateX(-50%)`, not `translateX( -50% )`
/// (RmlUi's transform parser rejects the spaced form).
std::string JoinTokens(const std::vector<CssToken>& tokens)
{
	std::string out;
	int paren_depth = 0;
	for (const auto& t : tokens)
	{
		if (t.kind == ETokenKind::Function)
		{
			if (!out.empty() && paren_depth == 0) out += ' ';
			out += t.text;
			out += '(';
			++paren_depth;
			continue;
		}
		if (t.kind == ETokenKind::RParen)
		{
			out += ')';
			if (paren_depth > 0) --paren_depth;
			continue;
		}
		if (t.kind == ETokenKind::Comma)
		{
			out += ',';
			continue;
		}
		if (!out.empty() && paren_depth == 0) out += ' ';
		out += t.text;
	}
	return out;
}

std::string JoinSelector(const std::vector<CssToken>& selector)
{
	return JoinTokens(selector);
}

std::string RenderValue(const std::vector<CssToken>& value)
{
	return JoinTokens(value);
}

void EmitDeclarations(const std::vector<CssDeclaration>& decls, std::string& out)
{
	for (const auto& d : decls)
	{
		out += "\t";
		out += d.property;
		out += ": ";
		out += RenderValue(d.value);
		out += ";\n";
	}
}

void EmitRule(const CssRule& rule, std::string& out)
{
	if (rule.selector.empty()) return;
	out += JoinSelector(rule.selector);
	out += " {\n";
	EmitDeclarations(rule.declarations, out);
	out += "}\n";
}

} // namespace

std::string RcssEmitter::Emit(const CssStylesheet& stylesheet)
{
	std::string out;
	for (const auto& at : stylesheet.at_rules)
	{
		if (at.kind == EAtRuleKind::Unknown) continue;
		out += "@";
		out += at.name;
		for (const auto& t : at.prelude)
			out += (t.kind == ETokenKind::Function) ? " " + t.text + "(" : " " + t.text;
		out += " {\n";
		for (const auto& rule : at.rules)
			EmitRule(rule, out);
		EmitDeclarations(at.declarations, out);
		out += "}\n";
	}
	for (const auto& rule : stylesheet.rules)
		EmitRule(rule, out);
	return out;
}

} // namespace MXRender::Tool::CssToRcss
