#include "RcssWhitelist.h"

#include <cstring>
#include <iterator>
#include <vector>

namespace MXRender::Tool::CssToRcss {

// ---------------------------------------------------------------------------
// Table derived from RmlUi 6.3 StyleSheetSpecification.cpp (RegisterProperty /
// RegisterShorthand, lines 262-433). Keywords are RmlUi's own lists.
// ---------------------------------------------------------------------------
static const RcssPropertySpec kSpecs[] = {
	// Box model
	{ "margin-top", EValueFamily::LengthPercent, "auto" },
	{ "margin-right", EValueFamily::LengthPercent, "auto" },
	{ "margin-bottom", EValueFamily::LengthPercent, "auto" },
	{ "margin-left", EValueFamily::LengthPercent, "auto" },
	{ "margin", EValueFamily::Shorthand, "" },
	{ "padding-top", EValueFamily::LengthPercent, "" },
	{ "padding-right", EValueFamily::LengthPercent, "" },
	{ "padding-bottom", EValueFamily::LengthPercent, "" },
	{ "padding-left", EValueFamily::LengthPercent, "" },
	{ "padding", EValueFamily::Shorthand, "" },
	{ "border-top-width", EValueFamily::Length, "" },
	{ "border-right-width", EValueFamily::Length, "" },
	{ "border-bottom-width", EValueFamily::Length, "" },
	{ "border-left-width", EValueFamily::Length, "" },
	{ "border-width", EValueFamily::Shorthand, "" },
	{ "border-top-color", EValueFamily::Color, "" },
	{ "border-right-color", EValueFamily::Color, "" },
	{ "border-bottom-color", EValueFamily::Color, "" },
	{ "border-left-color", EValueFamily::Color, "" },
	{ "border-color", EValueFamily::Shorthand, "" },
	{ "border-top", EValueFamily::Shorthand, "" },
	{ "border-right", EValueFamily::Shorthand, "" },
	{ "border-bottom", EValueFamily::Shorthand, "" },
	{ "border-left", EValueFamily::Shorthand, "" },
	{ "border", EValueFamily::Shorthand, "" },
	{ "border-top-left-radius", EValueFamily::Length, "" },
	{ "border-top-right-radius", EValueFamily::Length, "" },
	{ "border-bottom-right-radius", EValueFamily::Length, "" },
	{ "border-bottom-left-radius", EValueFamily::Length, "" },
	{ "border-radius", EValueFamily::Shorthand, "" },

	// Positioning / layout
	{ "display", EValueFamily::Keyword,
	  "none, block, inline, inline-block, flow-root, flex, inline-flex, table, inline-table, table-row, table-row-group, table-column, table-column-group, table-cell" },
	{ "position", EValueFamily::Keyword, "static, relative, absolute, fixed" },
	{ "top", EValueFamily::LengthPercent, "auto" },
	{ "right", EValueFamily::LengthPercent, "auto" },
	{ "bottom", EValueFamily::LengthPercent, "auto" },
	{ "left", EValueFamily::LengthPercent, "auto" },
	{ "inset", EValueFamily::Shorthand, "" },
	{ "float", EValueFamily::Keyword, "none, left, right" },
	{ "clear", EValueFamily::Keyword, "none, left, right, both" },
	{ "box-sizing", EValueFamily::Keyword, "content-box, border-box" },
	{ "z-index", EValueFamily::Number, "auto" },
	{ "width", EValueFamily::LengthPercent, "auto" },
	{ "min-width", EValueFamily::LengthPercent, "" },
	{ "max-width", EValueFamily::LengthPercent, "none" },
	{ "height", EValueFamily::LengthPercent, "auto" },
	{ "min-height", EValueFamily::LengthPercent, "" },
	{ "max-height", EValueFamily::LengthPercent, "none" },
	{ "line-height", EValueFamily::NumberLengthPercent, "" },
	{ "vertical-align", EValueFamily::LengthPercent,
	  "baseline, middle, sub, super, text-top, text-bottom, top, center, bottom" },
	{ "overflow-x", EValueFamily::Keyword, "visible, hidden, auto, scroll" },
	{ "overflow-y", EValueFamily::Keyword, "visible, hidden, auto, scroll" },
	{ "overflow", EValueFamily::Shorthand, "" },
	{ "clip", EValueFamily::Number, "auto, none, always" },
	{ "visibility", EValueFamily::Keyword, "visible, hidden" },
	{ "text-overflow", EValueFamily::String_, "clip, ellipsis" },

	// Colors / background
	{ "background-color", EValueFamily::Color, "" },
	{ "background", EValueFamily::Shorthand, "" },
	{ "color", EValueFamily::Color, "" },
	{ "caret-color", EValueFamily::Color, "auto" },
	{ "image-color", EValueFamily::Color, "" },
	{ "opacity", EValueFamily::Number, "" },

	// Fonts
	{ "font-family", EValueFamily::String_, "" },
	{ "font-style", EValueFamily::Keyword, "normal, italic" },
	{ "font-weight", EValueFamily::Number, "normal, bold" },
	{ "font-size", EValueFamily::LengthPercent, "" },
	{ "font-kerning", EValueFamily::Keyword, "auto, normal, none" },
	{ "letter-spacing", EValueFamily::Length, "normal" },
	{ "font", EValueFamily::Shorthand, "" },

	// Text
	{ "text-align", EValueFamily::Keyword, "left, right, center, justify" },
	{ "text-decoration", EValueFamily::Keyword, "none, underline, overline, line-through" },
	{ "text-transform", EValueFamily::Keyword, "none, capitalize, uppercase, lowercase" },
	{ "white-space", EValueFamily::Keyword, "normal, pre, nowrap, pre-wrap, pre-line" },
	{ "word-break", EValueFamily::Keyword, "normal, break-all, break-word" },

	// Flexbox
	{ "row-gap", EValueFamily::LengthPercent, "" },
	{ "column-gap", EValueFamily::LengthPercent, "" },
	{ "gap", EValueFamily::Shorthand, "" },
	{ "align-content", EValueFamily::Keyword, "flex-start, flex-end, center, space-between, space-around, space-evenly, stretch" },
	{ "align-items", EValueFamily::Keyword, "flex-start, flex-end, center, baseline, stretch" },
	{ "align-self", EValueFamily::Keyword, "auto, flex-start, flex-end, center, baseline, stretch" },
	{ "flex-basis", EValueFamily::LengthPercent, "auto" },
	{ "flex-direction", EValueFamily::Keyword, "row, row-reverse, column, column-reverse" },
	{ "flex-grow", EValueFamily::Number, "" },
	{ "flex-shrink", EValueFamily::Number, "" },
	{ "flex-wrap", EValueFamily::Keyword, "nowrap, wrap, wrap-reverse" },
	{ "justify-content", EValueFamily::Keyword, "flex-start, flex-end, center, space-between, space-around, space-evenly" },
	{ "flex", EValueFamily::Shorthand, "" },
	{ "flex-flow", EValueFamily::Shorthand, "" },

	// Interaction / misc
	{ "cursor", EValueFamily::String_, "" },
	{ "drag", EValueFamily::Keyword, "none, drag, drag-drop, block, clone" },
	{ "tab-index", EValueFamily::Keyword, "none, auto" },
	{ "focus", EValueFamily::Keyword, "none, auto" },
	{ "nav-up", EValueFamily::String_, "none, auto, horizontal, vertical, tree-order" },
	{ "nav-right", EValueFamily::String_, "none, auto, horizontal, vertical, tree-order" },
	{ "nav-down", EValueFamily::String_, "none, auto, horizontal, vertical, tree-order" },
	{ "nav-left", EValueFamily::String_, "none, auto, horizontal, vertical, tree-order" },
	{ "nav", EValueFamily::Shorthand, "" },
	{ "scrollbar-margin", EValueFamily::Length, "" },
	{ "overscroll-behavior", EValueFamily::Keyword, "auto, contain" },
	{ "pointer-events", EValueFamily::Keyword, "none, auto" },

	// Perspective / transform / effects (RmlUi parses the values itself)
	{ "perspective", EValueFamily::Length, "none" },
	{ "perspective-origin-x", EValueFamily::LengthPercent, "left, center, right" },
	{ "perspective-origin-y", EValueFamily::LengthPercent, "top, center, bottom" },
	{ "perspective-origin", EValueFamily::Shorthand, "" },
	{ "transform", EValueFamily::Complex, "" },
	{ "transform-origin-x", EValueFamily::LengthPercent, "left, center, right" },
	{ "transform-origin-y", EValueFamily::LengthPercent, "top, center, bottom" },
	{ "transform-origin-z", EValueFamily::Length, "" },
	{ "transform-origin", EValueFamily::Shorthand, "" },
	{ "transition", EValueFamily::Complex, "" },
	{ "animation", EValueFamily::Complex, "" },
	{ "decorator", EValueFamily::Complex, "" },
	{ "mask-image", EValueFamily::Complex, "" },
	{ "font-effect", EValueFamily::Complex, "" },
	{ "filter", EValueFamily::Complex, "" },
	{ "backdrop-filter", EValueFamily::Complex, "" },
	{ "box-shadow", EValueFamily::Complex, "" },
	{ "fill-image", EValueFamily::String_, "" },

	// RmlUi internals
	{ "-rmlui-language", EValueFamily::String_, "" },
	{ "-rmlui-direction", EValueFamily::Keyword, "auto, ltr, rtl" },
};

static const char* kBorderStyleKeywords[] = {
	"none", "hidden", "solid", "dashed", "dotted", "double",
	"groove", "ridge", "inset", "outset",
};

static const struct { const char* name; const char* hex; } kColorNames[] = {
	{ "black", "#000000" }, { "white", "#ffffff" }, { "red", "#ff0000" },
	{ "green", "#008000" }, { "lime", "#00ff00" }, { "blue", "#0000ff" },
	{ "yellow", "#ffff00" }, { "cyan", "#00ffff" }, { "aqua", "#00ffff" },
	{ "magenta", "#ff00ff" }, { "fuchsia", "#ff00ff" }, { "gray", "#808080" },
	{ "grey", "#808080" }, { "silver", "#c0c0c0" }, { "maroon", "#800000" },
	{ "olive", "#808000" }, { "purple", "#800080" }, { "teal", "#008080" },
	{ "navy", "#000080" }, { "orange", "#ffa500" }, { "pink", "#ffc0cb" },
	{ "brown", "#a52a2a" }, { "gold", "#ffd700" },
};

const RcssPropertySpec* RcssWhitelist::Find(const std::string& lower_name)
{
	for (const auto& spec : kSpecs)
		if (lower_name == spec.name)
			return &spec;
	return nullptr;
}

const std::vector<RcssPropertySpec>& RcssWhitelist::All()
{
	static const std::vector<RcssPropertySpec> kAll(std::begin(kSpecs), std::end(kSpecs));
	return kAll;
}

bool RcssWhitelist::IsKeywordValid(const RcssPropertySpec& spec, const std::string& lower_value)
{
	// Keyword lists are also attached to non-Keyword families (e.g. width: auto,
	// margin-top: auto, max-width: none, z-index: auto) — a bare ident value
	// must be validated against them regardless of family.
	if (!spec.keywords[0])
		return false;
	// RmlUi keyword lists may carry "normal=400" aliases — compare the part before '='.
	std::string value = lower_value;
	size_t eq = value.find('=');
	if (eq != std::string::npos) value = value.substr(0, eq);

	const char* cur = spec.keywords;
	while (*cur)
	{
		while (*cur == ' ' || *cur == '\t') ++cur;   // skip spaces after commas
		const char* end = cur;
		while (*end && *end != ',') ++end;
		if ((size_t)(end - cur) == value.size() && std::string(cur, end) == value)
			return true;
		cur = *end ? end + 1 : end;
	}
	return false;
}

bool RcssWhitelist::IsBorderStyleKeyword(const std::string& lower_value)
{
	for (const char* kw : kBorderStyleKeywords)
		if (lower_value == kw)
			return true;
	return false;
}

const char* RcssWhitelist::ColorNameToHex(const std::string& lower_name)
{
	for (const auto& c : kColorNames)
		if (lower_name == c.name)
			return c.hex;
	return nullptr;
}

} // namespace MXRender::Tool::CssToRcss
