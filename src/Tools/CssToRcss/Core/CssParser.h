#pragma once
#ifndef _CSS_PARSER_
#define _CSS_PARSER_

#include "CssAst.h"
#include <string>

namespace MXRender::Tool::CssToRcss {

/// Hand-rolled CSS tokenizer + recursive-descent parser for the CSS subset
/// that can plausibly convert to RCSS. Tolerant: every malformed construct
/// becomes a CssIssue (with line), never aborts parsing.
///
/// Design notes:
///  - Selectors are kept as raw token runs (verbatim, case preserved) —
///    RmlUi accepts standard CSS selectors, and re-encoding them risks
///    corrupting id/class names.
///  - Declaration values are token runs with nesting depth tracked
///    (calc(...) etc. parse as functions and are rejected later by the
///    converter with a targeted message).
///  - Numbers fuse their unit ("12px", "50%", "1.5em") into one token.
class CssParser
{
public:
	static CssStylesheet Parse(const std::string& css_text);

private:
	explicit CssParser(const std::string& text);

	std::vector<CssToken> Tokenize();
	void SkipTrivia();
	bool AtEnd() const { return Peek().kind == ETokenKind::EndOfFile; }
	const CssToken& Peek(int lookahead = 0) const;
	const CssToken& Take();
	const CssToken& Expect(ETokenKind kind);   // records an issue on mismatch, returns token anyway

	CssRule ParseQualifiedRule();
	std::vector<CssDeclaration> ParseDeclarationBlock(int& out_line);
	CssDeclaration ParseDeclaration();
	void ParseAtRule(CssStylesheet& out);
	void AddIssue(int line, const std::string& message);

	const std::string& m_text;
	std::vector<CssToken> m_tokens;
	size_t m_cursor = 0;
	CssStylesheet m_out;
};

} // namespace MXRender::Tool::CssToRcss

#endif // !_CSS_PARSER_
