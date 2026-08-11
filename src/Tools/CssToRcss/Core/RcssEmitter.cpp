#include "RcssEmitter.h"

namespace MXRender::Tool::CssToRcss {

namespace {

std::string JoinSelector(const std::vector<CssToken>& selector)
{
	std::string out;
	for (const auto& t : selector)
	{
		if (t.kind == ETokenKind::Function) { out += t.text; out += '('; continue; }
		if (t.kind == ETokenKind::RParen) { out += ')'; continue; }
		if (!out.empty() && t.kind != ETokenKind::Comma) out += ' ';
		out += t.text;
	}
	return out;
}

std::string RenderValue(const std::vector<CssToken>& value)
{
	std::string out;
	for (const auto& t : value)
	{
		if (!out.empty()) out += ' ';
		if (t.kind == ETokenKind::Function) { out += t.text; out += '('; }
		else { out += t.text; }
	}
	return out;
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
