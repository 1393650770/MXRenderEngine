#pragma once
#ifndef _CSS_AST_
#define _CSS_AST_

#include "CssToken.h"
#include <string>
#include <vector>

namespace MXRender::Tool::CssToRcss {

/// One `prop: value` declaration. Value tokens are kept in source order
/// (whitespace dropped — the emitter re-joins with single spaces).
struct CssDeclaration {
	std::string property;                 // lowercased
	std::vector<CssToken> value;          // non-whitespace tokens (functions hold name, '(' is implicit)
	bool important = false;               // !important seen
	int line = 0;
};

/// A qualified rule: selector token run + declarations.
struct CssRule {
	std::vector<CssToken> selector;       // non-whitespace tokens, verbatim (case preserved)
	std::vector<CssDeclaration> declarations;
	int line = 0;
};

enum class EAtRuleKind {
	Media,        // @media <cond> { rules }
	Keyframes,    // @keyframes <name> { keyframe rules }
	Block,        // @font-face / @decorator / @spritesheet { declarations }
	Unknown,      // anything else — skipped with a warning
};

struct CssAtRule {
	EAtRuleKind kind = EAtRuleKind::Unknown;
	std::string name;                     // lowercased, no '@'
	std::vector<CssToken> prelude;        // after the name, up to '{'
	std::vector<CssRule> rules;           // Media/Keyframes bodies
	std::vector<CssDeclaration> declarations; // Block bodies
	int line = 0;
};

struct CssIssue {
	int line = 0;
	std::string selector;                 // context ("" for top level)
	std::string message;
};

struct CssStylesheet {
	std::vector<CssRule> rules;
	std::vector<CssAtRule> at_rules;
	std::vector<CssIssue> issues;         // parse-level issues (recoverable)
};

} // namespace MXRender::Tool::CssToRcss

#endif // !_CSS_AST_
