#include "RcssEmitter.h"

namespace MXRender::Tool::CssToRcss {

// =========================================================================
// Token joining (public - shared with the editor's UIRcssParser)
//
// Joins tokens with single spaces EXCEPT:
//  - inside function parentheses - `translateX(-50%)` stays glued (RmlUi's
//    transform parser rejects the spaced form)
//  - inside attribute brackets - `[type="range"]` stays glued (a space after
//    `input` turns it into a descendant combinator that never matches)
//  - around ':' - `#id:hover` stays glued (the spaced form also reads as a
//    descendant combinator and the hover never fires)
// =========================================================================

std::string JoinTokens(const std::vector<CssToken>& tokens)
{
	std::string out;
	int paren_depth = 0, bracket_depth = 0;
	bool glue_next = false;   // a ':' glued - the next token must not be spaced
	for (const auto& t : tokens)
	{
		const bool inside = paren_depth > 0 || bracket_depth > 0;
		switch (t.kind)
		{
		case ETokenKind::Function:
			if (!out.empty() && !inside && !glue_next) out += ' ';
			out += t.text;
			// The tokenizer's function token is the bare ident ("translateX")
			// with the '(' consumed - except a lone '(' which is its own token.
			if (t.text.empty() || t.text.back() != '(') out += '(';
			++paren_depth;
			glue_next = false;
			continue;
		case ETokenKind::RParen:
			out += ')';
			if (paren_depth > 0) --paren_depth;
			glue_next = false;
			continue;
		case ETokenKind::LBracket:
			// Glue to the preceding selector part — `input[type="range"]`, not
			// `input [type="range"]` (the space reads as a descendant combinator).
			out += '[';
			++bracket_depth;
			glue_next = false;
			continue;
		case ETokenKind::RBracket:
			out += ']';
			if (bracket_depth > 0) --bracket_depth;
			glue_next = false;
			continue;
		case ETokenKind::Colon:
			out += ':';
			glue_next = true;
			continue;
		case ETokenKind::Comma:
			out += ',';
			glue_next = false;
			continue;
		default:
			if (!out.empty() && !inside && !glue_next) out += ' ';
			out += t.text;
			glue_next = false;
		}
	}
	return out;
}

std::string JoinSelector(const std::vector<CssToken>& selector)
{
	return JoinTokens(selector);
}

void EmitDeclarations(const std::vector<CssDeclaration>& decls, std::string& out)
{
	for (const auto& d : decls)
	{
		out += "\t";
		out += d.property;
		out += ": ";
		out += JoinTokens(d.value);
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
