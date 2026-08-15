#pragma once
#ifndef _RCSS_EMITTER_
#define _RCSS_EMITTER_

#include "CssAst.h"
#include <string>

namespace MXRender::Tool::CssToRcss {

/// Joins a token run into canonical text: single spaces between tokens,
/// except inside function parentheses (`translateX(-50%)` stays glued), inside
/// attribute brackets (`[type="range"]` stays glued) and around ':' (pseudo
/// `#id:hover` stays glued) - RmlUi's parsers reject or mis-parse the spaced
/// forms (`input [ type = "range" ]` reads as a descendant combinator).
std::string JoinTokens(const std::vector<CssToken>& tokens);
/// Selector-specialized alias of JoinTokens (keeps call sites intentful).
std::string JoinSelector(const std::vector<CssToken>& selector);

/// Deterministic canonical RCSS output:
///   selector1, selector2 {
///   \tprop: value;
///   }
/// Rules/declarations keep source order; values are token-normalized to
/// single spaces; at-rules re-emit their bodies (media/keyframes/block).
/// This form is the byte-level contract shared with golden tests and the
/// editor's import path (round-trip stable).
class RcssEmitter
{
public:
	static std::string Emit(const CssStylesheet& stylesheet);
};

} // namespace MXRender::Tool::CssToRcss

#endif // !_RCSS_EMITTER_
