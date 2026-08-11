#include "UIDocumentSerializer.h"

#include <cctype>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(UIDocModel)

namespace {

String EscapeAttr(const String& s)
{
	String out;
	out.reserve(s.size());
	for (char c : s)
	{
		switch (c)
		{
		case '&': out += "&amp;"; break;
		case '"': out += "&quot;"; break;
		case '<': out += "&lt;"; break;
		default: out += c;
		}
	}
	return out;
}

String EscapeText(const String& s)
{
	String out;
	out.reserve(s.size());
	for (char c : s)
	{
		switch (c)
		{
		case '&': out += "&amp;"; break;
		case '<': out += "&lt;"; break;
		case '>': out += "&gt;"; break;
		default: out += c;
		}
	}
	return out;
}

/// Collapse whitespace runs to single spaces and trim ends. Empty after
/// collapsing means the node is whitespace-only (dropped by the emitter).
String CollapseWhitespace(const String& s)
{
	String out;
	out.reserve(s.size());
	bool pending_space = false;
	for (char c : s)
	{
		if (std::isspace((unsigned char)c))
		{
			pending_space = !out.empty();
			continue;
		}
		if (pending_space) { out += ' '; pending_space = false; }
		out += c;
	}
	return out;
}

String JoinSelectors(const Vector<String>& selectors)
{
	String out;
	for (size_t i = 0; i < selectors.size(); ++i)
	{
		if (i > 0) out += ", ";
		out += selectors[i];
	}
	return out;
}

void EmitRcssRules(const Vector<UIRuleSet>& rules, String& out)
{
	for (const auto& rule : rules)
	{
		if (rule.selectors.empty()) continue;
		out += JoinSelectors(rule.selectors);
		out += " {\n";
		for (const auto& p : rule.properties)
		{
			out += "\t";
			out += p.name;
			out += ": ";
			out += p.value;
			out += ";\n";
		}
		out += "}\n";
	}
}

void EmitNode(const UIDocumentNode& node, int depth, String& out)
{
	const String indent(static_cast<size_t>(depth), '\t');

	if (node.type == ENodeType::Comment)
	{
		out += indent;
		out += "<!--";
		out += node.text;
		out += "-->\n";
		return;
	}
	if (node.type == ENodeType::Text)
	{
		String text = CollapseWhitespace(node.text);
		if (text.empty()) return;   // whitespace-only node dropped
		out += indent;
		out += EscapeText(text);
		out += "\n";
		return;
	}

	// Element
	out += indent;
	out += '<';
	out += node.tag_name;
	for (const auto& attr : node.attributes)
	{
		out += ' ';
		out += attr.name;
		out += "=\"";
		out += EscapeAttr(attr.value);
		out += '"';
	}
	if (node.children.empty())
	{
		out += " />\n";
		return;
	}
	out += ">\n";
	for (const auto& child : node.children)
		EmitNode(*child, depth + 1, out);
	out += indent;
	out += "</";
	out += node.tag_name;
	out += ">\n";
}

} // namespace

String UIDocumentSerializer::SerializeRml(const UIDocument& doc)
{
	String out;
	out += "<rml>\n";
	out += "\t<head>\n";
	out += "\t\t<title>";
	out += EscapeText(doc.title);
	out += "</title>\n";
	for (const auto& href : doc.stylesheets)
	{
		out += "\t\t<link type=\"text/css\" href=\"";
		out += EscapeAttr(href);
		out += "\" />\n";
	}
	out += "\t</head>\n";
	out += "\t<body";
	for (const auto& attr : doc.body_attributes)
	{
		out += ' ';
		out += attr.name;
		out += "=\"";
		out += EscapeAttr(attr.value);
		out += '"';
	}
	out += ">\n";
	for (const auto& node : doc.nodes)
		EmitNode(*node, 2, out);
	out += "\t</body>\n";
	out += "</rml>\n";
	return out;
}

String UIDocumentSerializer::SerializeRcss(const UIStyleSheet& stylesheet)
{
	String out;
	for (const auto& at : stylesheet.at_rules)
	{
		if (at.kind == EAtRuleKind::Unknown) continue;
		out += "@";
		out += at.name;
		if (!at.prelude.empty())
		{
			out += ' ';
			out += at.prelude;
		}
		out += " {\n";
		EmitRcssRules(at.rules, out);
		for (const auto& p : at.properties)
		{
			out += "\t";
			out += p.name;
			out += ": ";
			out += p.value;
			out += ";\n";
		}
		out += "}\n";
	}
	EmitRcssRules(stylesheet.rules, out);
	return out;
}

MYRENDERER_END_NAMESPACE // UIDocModel
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
