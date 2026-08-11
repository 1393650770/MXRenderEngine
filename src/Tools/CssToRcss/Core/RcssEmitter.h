#pragma once
#ifndef _RCSS_EMITTER_
#define _RCSS_EMITTER_

#include "CssAst.h"
#include <string>

namespace MXRender::Tool::CssToRcss {

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
