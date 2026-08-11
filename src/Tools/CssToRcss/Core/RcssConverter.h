#pragma once
#ifndef _RCSS_CONVERTER_
#define _RCSS_CONVERTER_

#include "CssAst.h"
#include "RcssWhitelist.h"

namespace MXRender::Tool::CssToRcss {

/// Converts a parsed CSS stylesheet into RCSS-safe form:
///  - Whitelisted properties pass through (RmlUi resolves shorthands itself);
///    border-* shorthands are expanded (width + color, style keywords dropped).
///  - Colors are normalized to #rrggbb/#rrggbbaa (hex, rgb()/hsl(), names).
///  - Unsupported constructs (grid, calc(), var(), sticky, url(), ...) are
///    dropped with targeted, line-numbered warnings.
///  - !important is stripped (flag kept in the declaration for the emitter).
///
/// Mutates the stylesheet in place (invalid declarations removed) and appends
/// conversion issues to stylesheet.issues.
class RcssConverter
{
public:
	static void Convert(CssStylesheet& stylesheet);

private:
	struct Ctx {
		std::string selector;   // current selector context for diagnostics
		CssStylesheet& out;
	};

	static void ConvertRule(CssRule& rule, Ctx& ctx);
	static void ConvertDeclarations(std::vector<CssDeclaration>& decls, Ctx& ctx);
	/// Returns false when the declaration must be dropped.
	static bool ConvertDeclaration(CssDeclaration& decl, Ctx& ctx, std::vector<CssDeclaration>& out);
	/// Color-family values → normalized hex; false on failure.
	static bool ConvertColorValue(CssDeclaration& decl, Ctx& ctx);
	/// Validate a value against the property's family; false drops the declaration.
	static bool ValidateValue(CssDeclaration& decl, const RcssPropertySpec& spec, Ctx& ctx);
	static bool IsRejected(const CssDeclaration& decl, Ctx& ctx, std::string& reason);
};

} // namespace MXRender::Tool::CssToRcss

#endif // !_RCSS_CONVERTER_
