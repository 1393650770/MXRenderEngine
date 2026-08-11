#include "RcssConverter.h"
#include "RcssWhitelist.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace MXRender::Tool::CssToRcss {

namespace {

std::string Lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return (char)std::tolower(c); });
	return s;
}

std::string JoinTokens(const std::vector<CssToken>& tokens)
{
	std::string out;
	for (const auto& t : tokens)
	{
		if (!out.empty()) out += ' ';
		out += t.text;
	}
	return out;
}

bool IsHexDigit(char c)
{
	return std::isxdigit((unsigned char)c);
}

/// #abc → #aabbcc, #abcd → #aabbccdd (4/8-digit stays).
bool ExpandHashColor(const std::string& in, std::string& out)
{
	if (in.empty() || in[0] != '#') return false;
	const std::string digits = in.substr(1);
	if (digits.size() == 3 || digits.size() == 4)
	{
		std::string doubled;
		doubled.reserve(digits.size() * 2);
		for (char c : digits)
		{
			if (!IsHexDigit(c)) return false;
			doubled += c; doubled += c;
		}
		out = "#" + doubled;
		return true;
	}
	if (digits.size() == 6 || digits.size() == 8)
	{
		for (char c : digits)
			if (!IsHexDigit(c)) return false;
		out = in;
		return true;
	}
	return false;
}

double ParseNum(const std::string& s, bool& ok)
{
	ok = !s.empty();
	char* end = nullptr;
	double v = std::strtod(s.c_str(), &end);
	if (end == s.c_str() || *end != '\0') ok = false;
	return v;
}

/// rgb()/rgba()/hsl()/hsla() → #rrggbb / #rrggbbaa. Tokens after the Function
/// up to the matching RParen (already depth-tracked by the parser).
bool ConvertFunctionColor(const std::vector<CssToken>& value, std::string& out)
{
	const std::string fn = Lower(value[0].text);
	if (fn != "rgb" && fn != "rgba" && fn != "hsl" && fn != "hsla") return false;

	// Collect the numeric arguments (skip commas), keeping percent info:
	//   rgb/rgba: bare = 0-255, % = 0..1 fraction
	//   hsl/hsla: h bare = degrees (0-360), s/l bare or % = 0..1 fraction
	std::vector<double> args;
	std::vector<bool> is_pct;
	bool have_alpha = fn == "rgba" || fn == "hsla";
	for (size_t i = 1; i < value.size(); ++i)
	{
		const CssToken& t = value[i];
		if (t.kind == ETokenKind::Comma || t.kind == ETokenKind::RParen) continue;
		if (t.kind != ETokenKind::Number) return false;
		bool ok = false;
		std::string text = t.text;
		bool pct = false;
		if (!text.empty() && text.back() == '%') { pct = true; text.pop_back(); }
		double v = ParseNum(text, ok);
		if (!ok) return false;
		// Normalize percent args to 0..1 fractions here.
		args.push_back(pct ? v / 100.0 : v);
		is_pct.push_back(pct);
	}
	const size_t want = have_alpha ? 4 : 3;
	if (args.size() != want) return false;

	auto clamp01 = [](double v) { return std::max(0.0, std::min(1.0, v)); };
	auto to255 = [&](double v) { return (int)std::lround(clamp01(v) * 255.0); };

	int r, g, b;
	if (fn == "rgb" || fn == "rgba")
	{
		auto channel = [&](size_t i) { return is_pct[i] ? args[i] : args[i] / 255.0; };
		r = to255(channel(0));
		g = to255(channel(1));
		b = to255(channel(2));
	}
	else
	{
		// hsl
		double h = is_pct[0] ? args[0] * 360.0 : args[0];
		auto pct_or_pct = [&](size_t i) { return is_pct[i] ? args[i] : args[i] / 100.0; };
		double s = clamp01(pct_or_pct(1));
		double l = clamp01(pct_or_pct(2));
		auto hue2rgb = [](double p, double q, double t) {
			if (t < 0.0) t += 1.0;
			if (t > 1.0) t -= 1.0;
			if (t < 1.0 / 6.0) return p + (q - p) * 6.0 * t;
			if (t < 1.0 / 2.0) return q;
			if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6.0;
			return p;
		};
		double q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
		double p = 2.0 * l - q;
		r = to255(hue2rgb(p, q, h / 360.0 + 1.0 / 3.0));
		g = to255(hue2rgb(p, q, h / 360.0));
		b = to255(hue2rgb(p, q, h / 360.0 - 1.0 / 3.0));
	}

	char buf[16];
	if (have_alpha)
		std::snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", r, g, b, to255(args[3]));
	else
		std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
	out = buf;
	return true;
}

bool IsSupportedUnit(const std::string& unit)
{
	static const char* kUnits[] = { "px", "dp", "em", "rem", "x", "vw", "vh",
		"in", "cm", "mm", "pt", "pc", "deg", "rad", "%" };
	for (const char* u : kUnits)
		if (unit == u) return true;
	return false;
}

bool SplitNumberUnit(const std::string& text, std::string& number, std::string& unit)
{
	size_t i = 0;
	if (i < text.size() && (text[i] == '-' || text[i] == '+')) ++i;
	bool dot = false;
	while (i < text.size() && (std::isdigit((unsigned char)text[i]) || (text[i] == '.' && !dot)))
	{
		if (text[i] == '.') dot = true;
		++i;
	}
	if (i == 0 || (i == 1 && (text[0] == '-' || text[0] == '+' || text[0] == '.'))) return false;
	number = text.substr(0, i);
	unit = text.substr(i);
	return true;
}

bool IsBareNumber(const std::string& text)
{
	std::string num, unit;
	if (!SplitNumberUnit(text, num, unit)) return false;
	return unit.empty();
}

bool IsLength(const std::string& text, bool allow_percent, bool allow_bare)
{
	std::string num, unit;
	if (!SplitNumberUnit(text, num, unit)) return false;
	if (unit.empty()) return allow_bare;
	if (unit == "%") return allow_percent;
	return IsSupportedUnit(unit);
}

/// Value tokens → normalized text (single spaces, functions rendered as name(...)).
std::string RenderValue(const std::vector<CssToken>& value)
{
	std::string out;
	bool expect_paren = false;
	for (const auto& t : value)
	{
		if (!out.empty() && !expect_paren) out += ' ';
		expect_paren = false;
		if (t.kind == ETokenKind::Function) { out += t.text; out += '('; expect_paren = true; }
		else if (t.kind == ETokenKind::RParen) { out += ')'; }
		else if (t.kind == ETokenKind::Comma) { out += ','; }
		else { out += t.text; }
	}
	return out;
}

} // namespace

// =========================================================================
// Entry point
// =========================================================================

void RcssConverter::Convert(CssStylesheet& stylesheet)
{
	for (auto& rule : stylesheet.rules)
	{
		Ctx ctx{ "", stylesheet };
		ConvertRule(rule, ctx);
	}
	for (auto& at : stylesheet.at_rules)
	{
		Ctx ctx{ "@" + at.name + " " + JoinTokens(at.prelude), stylesheet };
		for (auto& rule : at.rules)
			ConvertRule(rule, ctx);
		if (at.kind == EAtRuleKind::Block)
			ConvertDeclarations(at.declarations, ctx);
		// Drop rules that lost every declaration (all properties rejected).
		at.rules.erase(std::remove_if(at.rules.begin(), at.rules.end(),
			[](const CssRule& r) { return r.declarations.empty(); }), at.rules.end());
	}
	stylesheet.rules.erase(std::remove_if(stylesheet.rules.begin(), stylesheet.rules.end(),
		[](const CssRule& r) { return r.declarations.empty(); }), stylesheet.rules.end());
}

void RcssConverter::ConvertRule(CssRule& rule, Ctx& ctx)
{
	ctx.selector = JoinTokens(rule.selector);
	if (ctx.selector.empty())
	{
		ctx.out.issues.push_back(CssIssue{ rule.line, "", "dropping rule with empty selector" });
		rule.declarations.clear();
		return;
	}
	ConvertDeclarations(rule.declarations, ctx);
	if (rule.declarations.empty())
		ctx.out.issues.push_back(CssIssue{ rule.line, ctx.selector, "dropping empty rule" });
}

void RcssConverter::ConvertDeclarations(std::vector<CssDeclaration>& decls, Ctx& ctx)
{
	std::vector<CssDeclaration> out;
	out.reserve(decls.size());
	for (auto& decl : decls)
		ConvertDeclaration(decl, ctx, out);
	decls = std::move(out);
}

// =========================================================================
// Rejections (targeted messages) — checked before the whitelist
// =========================================================================

bool RcssConverter::IsRejected(const CssDeclaration& decl, Ctx& ctx, std::string& reason)
{
	const std::string& p = decl.property;

	// Custom properties
	if (p.rfind("--", 0) == 0) { reason = "custom properties are not supported (no var() in RmlUi)"; return true; }
	// CSS Grid
	if (p == "grid" || p.rfind("grid-", 0) == 0 || p.rfind("grid-template", 0) == 0)
	{ reason = "CSS Grid is not supported by RmlUi"; return true; }
	if (p == "display")
	{
		const std::string v = Lower(RenderValue(decl.value));
		if (v == "grid" || v == "inline-grid")
		{ reason = "display:grid is not supported by RmlUi (use flex)"; return true; }
	}
	if (p == "position")
	{
		const std::string v = Lower(RenderValue(decl.value));
		if (v == "sticky") { reason = "position:sticky is not supported by RmlUi"; return true; }
	}
	// Images — RmlUi uses decorator: instead
	if (p == "background-image" || p == "background-size" || p == "background-position"
		|| p == "background-repeat" || p == "background-attachment")
	{ reason = "background image properties are not supported — use decorator: (see RmlUi docs)"; return true; }
	if (p == "text-shadow") { reason = "text-shadow is not supported — use font-effect:"; return true; }
	if (p == "outline" || p.rfind("outline-", 0) == 0)
	{ reason = "outline is not supported — use border"; return true; }
	if (p == "aspect-ratio") { reason = "aspect-ratio is not supported by RmlUi"; return true; }
	if (p == "object-fit") { reason = "object-fit is not supported by RmlUi"; return true; }
	if (p == "content") { reason = "the content property is not supported by RmlUi"; return true; }
	if (p == "border-style" || p.rfind("border-", 0) == 0 && p.find("-style") != std::string::npos)
	{ reason = "border-style is not supported by RmlUi (borders are width + color only)"; return true; }

	// Value-level: functions RmlUi cannot evaluate
	for (const auto& t : decl.value)
	{
		if (t.kind != ETokenKind::Function) continue;
		const std::string fn = Lower(t.text);
		if (fn == "calc" || fn == "clamp" || fn == "min" || fn == "max")
		{ reason = fn + "() is not supported by RmlUi — use a plain value"; return true; }
		if (fn == "var") { reason = "var() is not supported (no custom properties in RmlUi)"; return true; }
		if (fn == "url")
		{ reason = "url() is not supported — use decorator: with image-source"; return true; }
		if (fn == "linear-gradient" || fn == "radial-gradient" || fn == "conic-gradient")
		{ reason = fn + "() is not supported by RmlUi — use decorator:"; return true; }
		if (fn == "env") { reason = "env() is not supported by RmlUi"; return true; }
	}

	// Unknown custom filter functions etc. — let the whitelist handle those.
	return false;
}

// =========================================================================
// Per-declaration conversion
// =========================================================================

bool RcssConverter::ConvertDeclaration(CssDeclaration& decl, Ctx& ctx,
	std::vector<CssDeclaration>& out)
{
	auto drop = [&](const std::string& message) {
		ctx.out.issues.push_back(CssIssue{ decl.line, ctx.selector, decl.property + ": " + message });
		return false;
	};

	if (decl.important)
	{
		decl.important = false;
		ctx.out.issues.push_back(CssIssue{ decl.line, ctx.selector,
			decl.property + ": !important stripped (not supported by RmlUi)" });
	}
	if (decl.value.empty()) return drop("empty value dropped");

	std::string reason;
	if (IsRejected(decl, ctx, reason)) return drop(reason);

	const RcssPropertySpec* spec = RcssWhitelist::Find(decl.property);
	if (!spec)
		return drop("unknown property (not supported by RmlUi)");

	// border* shorthands: RmlUi's border has width + color only — expand,
	// dropping style keywords.
	static const char* kBorderSides[] = { "border-top", "border-right", "border-bottom", "border-left" };
	if (decl.property == "border")
	{
		std::vector<CssToken> widths, colors;
		bool style_dropped = false;
		for (const auto& t : decl.value)
		{
			if (t.kind == ETokenKind::Number) widths.push_back(t);
			else if (t.kind == ETokenKind::Hash || t.kind == ETokenKind::Ident || t.kind == ETokenKind::Function)
			{
				if (t.kind == ETokenKind::Ident && RcssWhitelist::IsBorderStyleKeyword(Lower(t.text)))
				{ style_dropped = true; continue; }
				colors.push_back(t);
			}
			else return drop("unsupported token in border shorthand");
		}
		if (widths.empty() && colors.empty()) return drop("border with no width or color");
		if (style_dropped)
			ctx.out.issues.push_back(CssIssue{ decl.line, ctx.selector,
				"border: style keywords (solid/dashed/...) not supported — dropped" });
		for (const char* side : kBorderSides)
		{
			if (!widths.empty())
			{
				CssDeclaration w = decl;
				w.property = std::string(side) + "-width";
				w.value = { widths[widths.size() > 1 ? 0 : 0] };  // keep it simple: first width for all sides
				ConvertDeclaration(w, ctx, out);                  // recursive (validates the length)
			}
			if (!colors.empty())
			{
				CssDeclaration c = decl;
				c.property = std::string(side) + "-color";
				c.value = { colors[0] };
				ConvertDeclaration(c, ctx, out);
			}
		}
		return true;
	}

	// Keyword-family values (or keyword values of other families, e.g. width: auto)
	if (decl.value.size() == 1 && decl.value[0].kind == ETokenKind::Ident)
	{
		const std::string lower = Lower(decl.value[0].text);
		if (RcssWhitelist::IsKeywordValid(*spec, lower))
		{
			decl.value[0].text = lower;
			out.push_back(std::move(decl));
			return true;
		}
	}

	switch (spec->family)
	{
	case EValueFamily::Keyword:
		return drop("invalid value '" + RenderValue(decl.value) + "' — expected one of: "
			+ (spec->keywords[0] ? spec->keywords : ""));
	case EValueFamily::Color:
	{
		if (!ConvertColorValue(decl, ctx)) return drop("invalid color value");
		out.push_back(std::move(decl));
		return true;
	}
	case EValueFamily::Length:
	case EValueFamily::LengthPercent:
	case EValueFamily::Number:
	case EValueFamily::NumberLengthPercent:
	{
		if (decl.value.size() != 1 || decl.value[0].kind != ETokenKind::Number)
			return drop("invalid value '" + RenderValue(decl.value) + "'");
		const std::string text = decl.value[0].text;
		const bool ok = (spec->family == EValueFamily::Number) ? IsBareNumber(text)
			: IsLength(text, spec->family == EValueFamily::LengthPercent || spec->family == EValueFamily::NumberLengthPercent,
				spec->family != EValueFamily::Length);
		if (!ok) return drop("invalid value '" + text + "'");
		out.push_back(std::move(decl));
		return true;
	}
	case EValueFamily::String_:
		// Quoted string, or a bare keyword (cursor: pointer — RmlUi accepts
		// both, unknown ones are ignored at runtime).
		if (decl.value.size() == 1
			&& (decl.value[0].kind == ETokenKind::String_ || decl.value[0].kind == ETokenKind::Ident))
		{
			out.push_back(std::move(decl));
			return true;
		}
		return drop("expected a quoted string");
	case EValueFamily::Shorthand:
	{
		// RmlUi's `background` shorthand only covers background-color.
		if (decl.property == "background")
		{
			if (!ConvertColorValue(decl, ctx)) return drop("invalid color value");
		}
		else
		{
			// A whole-value function color (border-color: rgb(...)) — convert
			// to hex; on failure fall through to per-token normalization.
			if (decl.value.size() > 1 && decl.value[0].kind == ETokenKind::Function
				&& ConvertColorValue(decl, ctx))
			{
				out.push_back(std::move(decl));
				return true;
			}
			// Normalize standalone color tokens inside shorthand values
			// (border-color: red blue → hex). Non-color tokens are untouched —
			// unknown idents (e.g. font-family names) simply stay.
			for (auto& t : decl.value)
			{
				if (t.kind == ETokenKind::Hash)
				{
					std::string hex;
					if (ExpandHashColor(t.text, hex)) t.text = hex;
				}
				else if (t.kind == ETokenKind::Ident)
				{
					const std::string lower = Lower(t.text);
					if (lower == "transparent") t.text = "transparent";
					else if (const char* hex = RcssWhitelist::ColorNameToHex(lower))
					{
						t.kind = ETokenKind::Hash;
						t.text = hex;
					}
				}
			}
		}
		out.push_back(std::move(decl));
		return true;
	}
	case EValueFamily::Complex:
	default:
		// RmlUi parses these itself — pass through.
		out.push_back(std::move(decl));
		return true;
	}
}

bool RcssConverter::ConvertColorValue(CssDeclaration& decl, Ctx& ctx)
{
	const CssToken& t = decl.value[0];

	if (decl.value.size() == 1)
	{
		if (t.kind == ETokenKind::Hash)
		{
			std::string hex;
			if (!ExpandHashColor(t.text, hex)) return false;
			decl.value[0].text = hex;
			return true;
		}
		if (t.kind == ETokenKind::Ident)
		{
			const std::string lower = Lower(t.text);
			if (lower == "transparent") { decl.value[0].text = "transparent"; return true; }
			const char* hex = RcssWhitelist::ColorNameToHex(lower);
			if (!hex) return false;
			decl.value[0].kind = ETokenKind::Hash;
			decl.value[0].text = hex;
			return true;
		}
		return false;
	}
	if (t.kind == ETokenKind::Function)
	{
		// Function + its argument tokens (up to the RParen) form one color.
		std::string hex;
		if (!ConvertFunctionColor(decl.value, hex)) return false;
		decl.value.clear();
		decl.value.push_back(CssToken{ ETokenKind::Hash, hex, decl.line, 0 });
		return true;
	}
	return false;
}

} // namespace MXRender::Tool::CssToRcss
