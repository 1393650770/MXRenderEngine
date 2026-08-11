#pragma once
#ifndef _CSS_TOKEN_
#define _CSS_TOKEN_

#include <string>

namespace MXRender::Tool::CssToRcss {

// CSS token kinds — a pragmatic subset of CSS Syntax Level 3 enough for
// stylesheets (no prelude/URL/function special-casing beyond what converters
// need). Every token carries line/column for diagnostics.
enum class ETokenKind {
	Ident,        // foo, --custom-prop
	AtKeyword,    // @media
	Function,     // rgb( — text holds the name WITHOUT '('
	RParen,       // )
	LBracket,     // [
	RBracket,     // ]
	LBrace,       // {
	RBrace,       // }
	Semicolon,    // ;
	Colon,        // :
	Comma,        // ,
	String_,      // "..." or '...' (text includes quotes)
	Number,       // 12, 12.5, -3 — unit fused: "12px", "50%", "1.5em"
	Hash,         // #abc / #aabbcc (any #-token; color in values, id in selectors)
	Delim,        // any single other char (e.g. '/', '!', '*', '.', '>')
	Whitespace,   // runs of spaces/tabs/newlines
	Comment,      // /* ... */ (skipped by the parser, never kept)
	CDO,          // <!--
	CDC,          // -->
	EndOfFile,
};

struct CssToken {
	ETokenKind kind = ETokenKind::EndOfFile;
	std::string text;   // exact source text (units fused into Number text)
	int line = 0;
	int column = 0;

	bool IsWhitespaceOrComment() const
	{
		return kind == ETokenKind::Whitespace || kind == ETokenKind::Comment;
	}
	bool IsIdentOrNumber() const
	{
		return kind == ETokenKind::Ident || kind == ETokenKind::Number;
	}
};

} // namespace MXRender::Tool::CssToRcss

#endif // !_CSS_TOKEN_
