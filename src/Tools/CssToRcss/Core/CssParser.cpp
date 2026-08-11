#include "CssParser.h"

#include <algorithm>
#include <cctype>

namespace MXRender::Tool::CssToRcss {

namespace {

bool IsIdentStart(unsigned char c)
{
	return std::isalpha(c) || c == '_' || c >= 0x80;   // non-ASCII treated as ident (UTF-8 bytes)
}
bool IsIdentChar(unsigned char c)
{
	return IsIdentStart(c) || std::isdigit(c) || c == '-';
}
bool IsDigit(unsigned char c) { return std::isdigit(c); }
bool IsNumberStart(unsigned char c) { return IsDigit(c) || c == '.'; }

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return (char)std::tolower(c); });
	return s;
}

} // namespace

CssStylesheet CssParser::Parse(const std::string& css_text)
{
	CssParser p(css_text);
	p.m_tokens = p.Tokenize();
	p.m_cursor = 0;

	while (!p.AtEnd())
	{
		p.SkipTrivia();
		if (p.AtEnd()) break;
		if (p.Peek().kind == ETokenKind::AtKeyword)
			p.ParseAtRule(p.m_out);
		else
			p.m_out.rules.push_back(p.ParseQualifiedRule());
	}
	return std::move(p.m_out);
}

CssParser::CssParser(const std::string& text) : m_text(text) {}

// =========================================================================
// Tokenizer
// =========================================================================

std::vector<CssToken> CssParser::Tokenize()
{
	std::vector<CssToken> out;
	size_t i = 0;
	int line = 1, col = 1;
	const auto advance = [&](int n = 1) {
		for (int k = 0; k < n; ++k)
		{
			if (i < m_text.size())
			{
				if (m_text[i] == '\n') { ++line; col = 1; }
				else ++col;
				++i;
			}
		}
	};
	const auto emit = [&](ETokenKind kind, const std::string& text, int l, int c) {
		out.push_back(CssToken{ kind, text, l, c });
	};

	while (i < m_text.size())
	{
		const unsigned char ch = (unsigned char)m_text[i];
		const int l = line, c = col;

		// Whitespace run
		if (std::isspace(ch))
		{
			size_t start = i;
			while (i < m_text.size() && std::isspace((unsigned char)m_text[i])) advance();
			emit(ETokenKind::Whitespace, m_text.substr(start, i - start), l, c);
			continue;
		}
		// Comment
		if (ch == '/' && i + 1 < m_text.size() && m_text[i + 1] == '*')
		{
			size_t start = i;
			advance(2);
			bool closed = false;
			while (i + 1 < m_text.size())
			{
				if (m_text[i] == '*' && m_text[i + 1] == '/') { advance(2); closed = true; break; }
				advance();
			}
			if (!closed)
			{
				emit(ETokenKind::Comment, m_text.substr(start), l, c);
				break;
			}
			emit(ETokenKind::Comment, m_text.substr(start, i - start), l, c);
			continue;
		}
		// CDO / CDC
		if (m_text.compare(i, 4, "<!--") == 0) { advance(4); emit(ETokenKind::CDO, "<!--", l, c); continue; }
		if (m_text.compare(i, 3, "-->") == 0) { advance(3); emit(ETokenKind::CDC, "-->", l, c); continue; }

		// String
		if (ch == '"' || ch == '\'')
		{
			const char quote = (char)ch;
			size_t start = i;
			advance();
			bool closed = false;
			while (i < m_text.size())
			{
				if (m_text[i] == '\\' && i + 1 < m_text.size()) { advance(2); continue; }
				if (m_text[i] == quote) { advance(); closed = true; break; }
				if (m_text[i] == '\n') break;   // unterminated
				advance();
			}
			if (!closed) AddIssue(l, "unterminated string");
			emit(ETokenKind::String_, m_text.substr(start, i - start), l, c);
			continue;
		}

		// At-keyword
		if (ch == '@')
		{
			size_t start = i;
			advance();
			while (i < m_text.size() && IsIdentChar((unsigned char)m_text[i])) advance();
			emit(ETokenKind::AtKeyword, m_text.substr(start, i - start), l, c);
			continue;
		}
		// Hash — eats id chars including '-' so `#main-body` stays one token
		if (ch == '#')
		{
			size_t start = i;
			advance();
			while (i < m_text.size() && IsIdentChar((unsigned char)m_text[i])) advance();
			emit(ETokenKind::Hash, m_text.substr(start, i - start), l, c);
			continue;
		}
		// Number (with fused unit, '%' included)
		if (IsNumberStart(ch) || (ch == '-' && i + 1 < m_text.size() && IsDigit((unsigned char)m_text[i + 1])))
		{
			size_t start = i;
			if (ch == '-') advance();
			while (i < m_text.size() && (IsDigit((unsigned char)m_text[i]) || m_text[i] == '.')) advance();
			while (i < m_text.size() && (IsIdentChar((unsigned char)m_text[i]) || m_text[i] == '%')) advance();  // unit
			emit(ETokenKind::Number, m_text.substr(start, i - start), l, c);
			continue;
		}
		// Ident or Function (ident immediately followed by '('); '-' starts an
		// ident when followed by another ident char (e.g. --custom-property).
		if (IsIdentStart(ch) || (ch == '-' && i + 1 < m_text.size()
			&& (IsIdentStart((unsigned char)m_text[i + 1]) || m_text[i + 1] == '-')))
		{
			size_t start = i;
			advance();
			while (i < m_text.size() && IsIdentChar((unsigned char)m_text[i])) advance();
			std::string name = m_text.substr(start, i - start);
			// whitespace between name and '(' is NOT allowed for a function
			if (i < m_text.size() && m_text[i] == '(')
			{
				advance();
				emit(ETokenKind::Function, name, l, c);
			}
			else
			{
				emit(ETokenKind::Ident, name, l, c);
			}
			continue;
		}

		// Single-char tokens
		ETokenKind kind = ETokenKind::Delim;
		switch (ch)
		{
		case '(': kind = ETokenKind::Function; break; // bare '(' — treated as function with empty name
		case ')': kind = ETokenKind::RParen; break;
		case '[': kind = ETokenKind::LBracket; break;
		case ']': kind = ETokenKind::RBracket; break;
		case '{': kind = ETokenKind::LBrace; break;
		case '}': kind = ETokenKind::RBrace; break;
		case ';': kind = ETokenKind::Semicolon; break;
		case ':': kind = ETokenKind::Colon; break;
		case ',': kind = ETokenKind::Comma; break;
		default: break;
		}
		emit(kind, m_text.substr(i, 1), l, c);
		advance();
	}
	emit(ETokenKind::EndOfFile, "", line, col);
	return out;
}

// =========================================================================
// Token cursor
// =========================================================================

void CssParser::SkipTrivia()
{
	while (!AtEnd() && Peek().IsWhitespaceOrComment())
		++m_cursor;
}

const CssToken& CssParser::Peek(int lookahead) const
{
	size_t idx = std::min(m_cursor + (size_t)lookahead, m_tokens.size() - 1);
	return m_tokens[idx];
}

const CssToken& CssParser::Take()
{
	const CssToken& t = m_tokens[m_cursor];
	if (m_cursor + 1 < m_tokens.size()) ++m_cursor;
	return t;
}

const CssToken& CssParser::Expect(ETokenKind kind)
{
	if (Peek().kind != kind)
		AddIssue(Peek().line, "expected token, found '" + (Peek().text.empty() ? "<eof>" : Peek().text) + "'");
	return Take();
}

void CssParser::AddIssue(int line, const std::string& message)
{
	m_out.issues.push_back(CssIssue{ line, "", message });
}

// =========================================================================
// Recursive descent
// =========================================================================

CssRule CssParser::ParseQualifiedRule()
{
	CssRule rule;
	rule.line = Peek().line;

	// Selector: token run up to '{', tracking ()[] and strings.
	int paren = 0, bracket = 0;
	while (!AtEnd())
	{
		const CssToken& t = Peek();
		if (t.kind == ETokenKind::LBrace && paren == 0 && bracket == 0)
			break;
		if (t.kind == ETokenKind::Function) ++paren;
		else if (t.kind == ETokenKind::RParen && paren > 0) --paren;
		else if (t.kind == ETokenKind::LBracket) ++bracket;
		else if (t.kind == ETokenKind::RBracket && bracket > 0) --bracket;
		if (t.kind == ETokenKind::RBrace)
		{
			AddIssue(t.line, "stray '}' — rule body closed early, ignoring the rest of the selector");
			break;
		}
		if (t.kind != ETokenKind::EndOfFile && !t.IsWhitespaceOrComment())
			rule.selector.push_back(t);
		++m_cursor;
	}
	if (AtEnd())
	{
		AddIssue(rule.line, "unterminated rule (missing '{')");
		return rule;
	}
	Expect(ETokenKind::LBrace);

	// Declaration block
	int block_line = 0;
	rule.declarations = ParseDeclarationBlock(block_line);

	if (rule.selector.empty())
		AddIssue(rule.line, "rule with empty selector");
	return rule;
}

std::vector<CssDeclaration> CssParser::ParseDeclarationBlock(int& out_line)
{
	std::vector<CssDeclaration> decls;
	out_line = Peek().line;

	while (!AtEnd())
	{
		SkipTrivia();
		const CssToken& t = Peek();
		if (t.kind == ETokenKind::RBrace) { ++m_cursor; break; }
		if (t.kind == ETokenKind::Semicolon) { ++m_cursor; continue; }
		if (t.kind == ETokenKind::AtKeyword)
		{
			AddIssue(t.line, "nested at-rule inside a declaration block is not supported");
			++m_cursor;
			continue;
		}
		if (t.kind == ETokenKind::Ident)
		{
			decls.push_back(ParseDeclaration());
			continue;
		}
		// Stray token — skip to ';' or '}' (tolerant recovery)
		AddIssue(t.line, "unexpected token in declaration block");
		++m_cursor;
		while (!AtEnd())
		{
			const CssToken& s = Peek();
			if (s.kind == ETokenKind::Semicolon || s.kind == ETokenKind::RBrace) break;
			++m_cursor;
		}
	}
	if (AtEnd())
		AddIssue(out_line, "unterminated declaration block (missing '}')");
	return decls;
}

CssDeclaration CssParser::ParseDeclaration()
{
	CssDeclaration decl;
	const CssToken& name = Take();   // Ident
	decl.property = Lower(name.text);
	decl.line = name.line;

	SkipTrivia();
	if (Peek().kind != ETokenKind::Colon)
	{
		AddIssue(name.line, "declaration '" + decl.property + "' missing ':'");
		return decl;
	}
	++m_cursor;   // ':'

	// Value: tokens up to ';' or '}' at depth 0, tracking ()/[]/strings.
	int paren = 0, bracket = 0;
	bool saw_value = false;
	while (!AtEnd())
	{
		const CssToken& t = Peek();
		if ((t.kind == ETokenKind::Semicolon || t.kind == ETokenKind::RBrace) && paren == 0 && bracket == 0)
			break;
		if (t.kind == ETokenKind::Function) ++paren;
		else if (t.kind == ETokenKind::RParen && paren > 0) --paren;
		else if (t.kind == ETokenKind::LBracket) ++bracket;
		else if (t.kind == ETokenKind::RBracket && bracket > 0) --bracket;
		if (t.kind == ETokenKind::EndOfFile) break;

		if (!t.IsWhitespaceOrComment())
		{
			// !important detection: '!' Delim followed by Ident "important"
			if (t.kind == ETokenKind::Delim && t.text == "!"
				&& Peek(1).kind == ETokenKind::Ident && Lower(Peek(1).text) == "important")
			{
				decl.important = true;
				++m_cursor; // consume '!'
				++m_cursor; // consume 'important'
				continue;
			}
			decl.value.push_back(t);
			saw_value = true;
		}
		++m_cursor;
	}
	if (Peek().kind == ETokenKind::Semicolon) ++m_cursor;
	if (AtEnd())
		AddIssue(decl.line, "unterminated declaration '" + decl.property + "' (missing ';')");
	(void)saw_value;
	return decl;
}

void CssParser::ParseAtRule(CssStylesheet& out)
{
	CssAtRule at;
	const CssToken& kw = Take();
	at.name = Lower(kw.text.substr(1));   // strip '@'
	at.line = kw.line;

	// Prelude tokens up to '{' (or ';' for statement at-rules like @import)
	int paren = 0, bracket = 0;
	while (!AtEnd())
	{
		const CssToken& t = Peek();
		if (t.kind == ETokenKind::LBrace && paren == 0 && bracket == 0) break;
		if (t.kind == ETokenKind::Semicolon) { ++m_cursor; break; }   // statement at-rule
		if (t.kind == ETokenKind::RBrace)
		{
			AddIssue(t.line, "stray '}' in @-rule prelude");
			break;
		}
		if (t.kind == ETokenKind::Function) ++paren;
		else if (t.kind == ETokenKind::RParen && paren > 0) --paren;
		else if (t.kind == ETokenKind::LBracket) ++bracket;
		else if (t.kind == ETokenKind::RBracket && bracket > 0) --bracket;
		if (!t.IsWhitespaceOrComment())
			at.prelude.push_back(t);
		++m_cursor;
	}

	if (AtEnd() || Peek().kind != ETokenKind::LBrace)
	{
		// Statement at-rule (no block): @import / @charset / unknown — skip.
		at.kind = EAtRuleKind::Unknown;
		AddIssue(at.line, "unsupported statement at-rule '@" + at.name + "' (skipped)");
		out.at_rules.push_back(std::move(at));
		return;
	}
	Expect(ETokenKind::LBrace);

	if (at.name == "media")
	{
		at.kind = EAtRuleKind::Media;
		while (!AtEnd())
		{
			SkipTrivia();
			if (Peek().kind == ETokenKind::RBrace) { ++m_cursor; break; }
			if (Peek().kind == ETokenKind::AtKeyword)
			{
				AddIssue(Peek().line, "nested at-rule inside @media skipped");
				++m_cursor;
				continue;
			}
			at.rules.push_back(ParseQualifiedRule());
		}
		if (AtEnd()) AddIssue(at.line, "unterminated @media block");
	}
	else if (at.name == "keyframes")
	{
		at.kind = EAtRuleKind::Keyframes;
		while (!AtEnd())
		{
			SkipTrivia();
			if (Peek().kind == ETokenKind::RBrace) { ++m_cursor; break; }
			if (Peek().kind == ETokenKind::Ident || Peek().kind == ETokenKind::Number)
			{
				CssRule keyframe;
				keyframe.line = Peek().line;
				while (!AtEnd())
				{
					const CssToken& t = Peek();
					if (t.kind == ETokenKind::LBrace) break;
					if (!t.IsWhitespaceOrComment())
						keyframe.selector.push_back(t);
					++m_cursor;
				}
				Expect(ETokenKind::LBrace);
				int bl = 0;
				keyframe.declarations = ParseDeclarationBlock(bl);
				at.rules.push_back(std::move(keyframe));
				continue;
			}
			AddIssue(Peek().line, "unexpected token in @keyframes");
			++m_cursor;
		}
		if (AtEnd()) AddIssue(at.line, "unterminated @keyframes block");
	}
	else if (at.name == "font-face" || at.name == "decorator" || at.name == "spritesheet")
	{
		at.kind = EAtRuleKind::Block;
		int bl = 0;
		at.declarations = ParseDeclarationBlock(bl);
	}
	else
	{
		at.kind = EAtRuleKind::Unknown;
		AddIssue(at.line, "unsupported block at-rule '@" + at.name + "' (skipped)");
		// Skip to matching '}'
		int depth = 1;
		while (!AtEnd() && depth > 0)
		{
			const CssToken& t = Take();
			if (t.kind == ETokenKind::LBrace) ++depth;
			else if (t.kind == ETokenKind::RBrace) --depth;
		}
	}
	out.at_rules.push_back(std::move(at));
}

} // namespace MXRender::Tool::CssToRcss
