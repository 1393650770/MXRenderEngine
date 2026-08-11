#pragma once
#ifndef _RCSS_WHITELIST_
#define _RCSS_WHITELIST_

#include <string>

namespace MXRender::Tool::CssToRcss {

/// Value families — drive validation + the editor's future typed property panel.
enum class EValueFamily {
	Color,               // colors (hex/rgb()/hsl()/name → converted to hex)
	Length,              // length with unit (px, dp, em, rem, x, vw, vh, in, cm, mm, pt, pc, deg, rad)
	LengthPercent,       // length or %
	Number,              // bare number
	NumberLengthPercent, // number / length / %
	Keyword,             // one of the property's keyword list
	String_,             // quoted string
	Complex,             // parsed by RmlUi itself (transform/transition/animation/decorator/...)
	Shorthand,           // RmlUi-supported shorthand — passed through (border family handled specially)
};

struct RcssPropertySpec {
	const char* name;
	EValueFamily family;
	const char* keywords;   // comma-separated keyword list (Keyword family only)
};

/// Authority: src/ThirdParty/RmlUi/Source/Core/StyleSheetSpecification.cpp
/// (RegisterProperty/RegisterShorthand blocks, lines 262-433). Longhands AND
/// shorthands live in one table — RmlUi resolves shorthands itself, so the
/// converter passes them through untouched except border-* (RmlUi's border
/// shorthand has no style component).
class RcssWhitelist
{
public:
	/// nullptr if the property is unknown to RmlUi.
	static const RcssPropertySpec* Find(const std::string& lower_name);

	/// Case-insensitive keyword check (value must be lowercased first).
	static bool IsKeywordValid(const RcssPropertySpec& spec, const std::string& lower_value);

	/// Border style keywords CSS allows that RmlUi does not (dropped with a warning).
	static bool IsBorderStyleKeyword(const std::string& lower_value);

	/// Built-in CSS color names → "#rrggbb" (transparent stays "transparent").
	static const char* ColorNameToHex(const std::string& lower_name);
};

} // namespace MXRender::Tool::CssToRcss

#endif // !_RCSS_WHITELIST_
