#include "RmlParser.h"

#include <cctype>
#include <cstdlib>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(UIDocModel)

namespace {

bool IsNameChar(char c)
{
	return std::isalnum((unsigned char)c) || c == '-' || c == '_' || c == ':'
		|| c == '.' || (unsigned char)c >= 0x80;
}

bool IsWhitespace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

String ToLower(String s)
{
	for (auto& c : s) c = (char)std::tolower((unsigned char)c);
	return s;
}

} // namespace

UIDocument RmlParser::Parse(const String& text, const String& file_path)
{
	RmlParser p(text, file_path);
	p.m_doc.file_path = file_path;

	size_t first_lt = text.find('<');
	if (first_lt == String::npos)
	{
		p.Error(1, "no '<' found — not an RML document");
		return std::move(p.m_doc);
	}
	p.m_pos = first_lt;

	for (;;)
	{
		if (p.m_pos >= p.m_text.size()) break;

		if (p.m_text[p.m_pos] != '<')
		{
			// whitespace/text between top-level tags — skip to the next '<'
			p.AdvanceTo(p.m_text.find('<', p.m_pos));
			continue;
		}
		if (p.m_text.compare(p.m_pos, 4, "<!--") == 0)
		{
			size_t end = p.m_text.find("-->", p.m_pos + 4);
			size_t stop = end == String::npos ? p.m_text.size() : end + 3;
			p.AdvanceTo(stop);
			continue;
		}
		if (p.m_text.compare(p.m_pos, 2, "<?") == 0 || p.m_text.compare(p.m_pos, 2, "<!") == 0)
		{
			size_t end = p.m_text.find('>', p.m_pos + 2);
			if (end == String::npos) { p.Error(p.m_line, "unterminated processing instruction"); break; }
			p.AdvanceTo(end + 1);
			continue;
		}
		if (p.m_text.compare(p.m_pos, 2, "</") == 0)
		{
			// close of <rml> — we don't track the rml element itself
			size_t end = p.m_text.find('>', p.m_pos + 2);
			if (end == String::npos) break;
			p.AdvanceTo(end + 1);
			continue;
		}

		// open tag: <head> or <body>
		UIDocumentNode tag_node;
		bool self_closing = false;
		if (!p.ConsumeTagOpen(&tag_node, self_closing))
			break;
		const String tag = ToLower(tag_node.tag_name);
		if (tag == "head")
			p.ParseContainer(tag, /*is_head=*/true);
		else if (tag == "body")
		{
			p.m_doc.body_attributes = std::move(tag_node.attributes);   // keep data-model etc.
			p.ParseContainer(tag, /*is_head=*/false);
		}
		else if (tag == "rml")
		{
			// Root element — contents (head/body) are handled by the loop.
		}
		else
		{
			p.Error(tag_node.source_line, "unexpected top-level element <" + tag + "> ignored");
			if (!self_closing)
				p.SkipElement(tag);
		}
	}
	if (p.m_doc.nodes.empty())
		p.Error(p.m_line, "no <body> found");
	return std::move(p.m_doc);
}

RmlParser::RmlParser(const String& text, const String& file_path)
	: m_text(text), m_file_path(file_path) {}

void RmlParser::AdvanceTo(size_t pos)
{
	for (; m_pos < pos && m_pos < m_text.size(); ++m_pos)
		if (m_text[m_pos] == '\n') ++m_line;
}

// =========================================================================
// Container (head/body) content
// =========================================================================

void RmlParser::ParseContainer(const String& container_tag, bool is_head)
{
	Vector<UIDocumentNode*> stack;   // open elements, innermost last

	for (;;)
	{
		if (m_pos >= m_text.size())
		{
			Error(m_line, "unterminated <" + container_tag + "> (auto-closed at EOF)");
			return;
		}
		if (m_text[m_pos] != '<')
		{
			// text: attach to the innermost open element; direct container
			// text (between head/body children) is dropped
			String text = ParseText();
			if (!text.empty() && !stack.empty())
			{
				auto node = std::make_unique<UIDocumentNode>();
				node->type = ENodeType::Text;
				node->text = text;
				node->source_line = m_line;
				node->parent = stack.back();
				stack.back()->children.push_back(std::move(node));
			}
			continue;
		}
		if (m_text.compare(m_pos, 4, "<!--") == 0)
		{
			size_t end = m_text.find("-->", m_pos + 4);
			size_t stop = end == String::npos ? m_text.size() : end + 3;
			if (!stack.empty())
			{
				auto node = std::make_unique<UIDocumentNode>();
				node->type = ENodeType::Comment;
				node->text = m_text.substr(m_pos + 4,
					stop - m_pos - 4 - (end == String::npos ? 0 : 3));
				node->source_line = m_line;
				node->parent = stack.back();
				stack.back()->children.push_back(std::move(node));
			}
			AdvanceTo(stop);
			continue;
		}
		if (m_text.compare(m_pos, 2, "<?") == 0 || m_text.compare(m_pos, 2, "<!") == 0)
		{
			size_t end = m_text.find('>', m_pos + 2);
			if (end == String::npos) { Error(m_line, "unterminated processing instruction"); return; }
			AdvanceTo(end + 1);
			continue;
		}
		if (m_text.compare(m_pos, 2, "</") == 0)
		{
			size_t end = m_text.find('>', m_pos + 2);
			if (end == String::npos) { Error(m_line, "unterminated close tag"); return; }
			String close = ToLower(m_text.substr(m_pos + 2, end - m_pos - 2));
			size_t b = close.find_first_not_of(" \t\r\n");
			size_t e = close.find_last_not_of(" \t\r\n");
			close = (b == String::npos) ? "" : close.substr(b, e - b + 1);
			AdvanceTo(end + 1);

			if (close == container_tag && stack.empty())
				return;   // container closed
			if (!stack.empty() && close == stack.back()->tag_name)
			{
				stack.pop_back();
			}
			else
			{
				bool matched = false;
				for (size_t i = stack.size(); i-- > 0;)
				{
					if (stack[i]->tag_name == close)
					{
						Error(m_line, "mismatched close tag </" + close + "> — auto-closed "
							+ std::to_string(stack.size() - 1 - i) + " element(s)");
						stack.resize(i);
						matched = true;
						break;
					}
				}
				if (!matched && close != container_tag)
					Error(m_line, "stray close tag </" + close + "> ignored");
			}
			continue;
		}

		// open tag
		int tag_line = m_line;
		auto node = std::make_unique<UIDocumentNode>();
		bool self_closing = false;
		if (!ConsumeTagOpen(node.get(), self_closing))
			return;
		node->source_line = tag_line;
		UIDocumentNode* raw = node.get();

		if (is_head)
		{
			const String tag = ToLower(raw->tag_name);
			if (tag == "title")
			{
				// Children aren't parsed yet — consume the text node directly.
				if (m_pos < m_text.size() && m_text[m_pos] != '<')
					m_doc.title = ParseText();
			}
			else if (tag == "link")
			{
				if (const String* href = raw->GetAttribute("href"))
					m_doc.stylesheets.push_back(*href);
			}
			if (!self_closing)
				SkipElement(tag);   // consume title/link content up to its close
			continue;   // head children are metadata, not kept in the tree
		}

		if (raw->tag_name.empty())
		{
			Error(tag_line, "empty tag name");
			continue;
		}

		if (stack.empty())
			m_doc.nodes.push_back(std::move(node));
		else
		{
			raw->parent = stack.back();
			stack.back()->children.push_back(std::move(node));
		}
		if (!self_closing)
			stack.push_back(raw);
	}
}

/// Consumes <tag ...> at m_pos; fills node (tag name + attributes). On
/// success m_pos sits after '>'. Self-closing (`/>`) sets out_self_closing.
bool RmlParser::ConsumeTagOpen(UIDocumentNode* node, bool& out_self_closing)
{
	out_self_closing = false;
	size_t gt = m_text.find('>', m_pos + 1);
	if (gt == String::npos)
	{
		Error(m_line, "unterminated tag (auto-closed at EOF)");
		AdvanceTo(m_text.size());
		return false;
	}
	String inner = m_text.substr(m_pos + 1, gt - m_pos - 1);
	if (!inner.empty() && inner.back() == '/')
	{
		out_self_closing = true;
		inner.pop_back();
	}

	size_t i = 0;
	while (i < inner.size() && IsWhitespace(inner[i])) ++i;
	size_t name_start = i;
	while (i < inner.size() && IsNameChar(inner[i])) ++i;
	node->tag_name = inner.substr(name_start, i - name_start);
	node->type = ENodeType::Element;
	node->source_line = m_line;

	while (i < inner.size())
	{
		while (i < inner.size() && IsWhitespace(inner[i])) ++i;
		if (i >= inner.size()) break;
		size_t attr_start = i;
		while (i < inner.size() && IsNameChar(inner[i])) ++i;
		String attr_name = inner.substr(attr_start, i - attr_start);
		if (attr_name.empty())
		{
			// bare '/' inside (e.g. "tag / >") — treat as self-closing and stop
			if (inner[i] == '/') { out_self_closing = true; break; }
			Error(m_line, "malformed attribute in <" + node->tag_name + ">");
			break;
		}
		while (i < inner.size() && IsWhitespace(inner[i])) ++i;
		String attr_value;
		if (i < inner.size() && inner[i] == '=')
		{
			++i;
			while (i < inner.size() && IsWhitespace(inner[i])) ++i;
			if (i < inner.size() && (inner[i] == '"' || inner[i] == '\''))
			{
				char quote = inner[i++];
				size_t vstart = i;
				while (i < inner.size() && inner[i] != quote) ++i;
				attr_value = inner.substr(vstart, i - vstart);
				if (i < inner.size()) ++i;   // closing quote
			}
			else
			{
				size_t vstart = i;
				while (i < inner.size() && !IsWhitespace(inner[i])) ++i;
				attr_value = inner.substr(vstart, i - vstart);
				Error(m_line, "unquoted attribute value for '" + attr_name + "'");
			}
		}
		attr_value = DecodeEntities(attr_value, m_line);
		bool replaced = false;
		for (auto& a : node->attributes)
		{
			if (a.name == attr_name) { a.value = attr_value; replaced = true; break; }
		}
		if (!replaced)
			node->attributes.push_back(UIAttribute{ attr_name, attr_value });
	}

	AdvanceTo(gt + 1);
	return true;
}

/// Skips everything up to and including </tag> (tolerant: nested same-tag
/// counts). Used for head metadata and unexpected top-level elements.
void RmlParser::SkipElement(const String& tag)
{
	int depth = 1;
	while (m_pos < m_text.size() && depth > 0)
	{
		size_t lt = m_text.find('<', m_pos);
		if (lt == String::npos) { AdvanceTo(m_text.size()); return; }
		if (m_text.compare(lt, 2, "</") == 0)
		{
			size_t gt = m_text.find('>', lt + 2);
			if (gt == String::npos) { AdvanceTo(m_text.size()); return; }
			String close = ToLower(m_text.substr(lt + 2, gt - lt - 2));
			size_t b = close.find_first_not_of(" \t\r\n");
			size_t e = close.find_last_not_of(" \t\r\n");
			close = (b == String::npos) ? "" : close.substr(b, e - b + 1);
			if (close == tag) --depth;
			AdvanceTo(gt + 1);
		}
		else if (m_text.compare(lt, 4, "<!--") == 0)
		{
			size_t end = m_text.find("-->", lt + 4);
			AdvanceTo(end == String::npos ? m_text.size() : end + 3);
		}
		else
		{
			size_t gt = m_text.find('>', lt + 1);
			if (gt == String::npos) { AdvanceTo(m_text.size()); return; }
			String inner = m_text.substr(lt + 1, gt - lt - 1);
			size_t sp = inner.find_first_of(" \t\r\n");
			String t = ToLower(sp == String::npos ? inner : inner.substr(0, sp));
			if (t == tag) ++depth;
			AdvanceTo(gt + 1);
		}
	}
}

String RmlParser::ParseText()
{
	size_t start = m_pos;
	while (m_pos < m_text.size() && m_text[m_pos] != '<')
		++m_pos;
	// count lines in the raw span
	for (size_t i = start; i < m_pos; ++i)
		if (m_text[i] == '\n') ++m_line;
	String raw = m_text.substr(start, m_pos - start);
	return DecodeEntities(raw, m_line);
}

String RmlParser::DecodeEntities(const String& raw, int line)
{
	String out;
	out.reserve(raw.size());
	for (size_t i = 0; i < raw.size(); ++i)
	{
		if (raw[i] != '&')
		{
			out += raw[i];
			continue;
		}
		size_t semi = raw.find(';', i + 1);
		if (semi == String::npos || semi - i > 10)
		{
			out += raw[i];
			continue;
		}
		String ent = raw.substr(i + 1, semi - i - 1);
		char decoded = 0;
		if (ent == "amp") decoded = '&';
		else if (ent == "lt") decoded = '<';
		else if (ent == "gt") decoded = '>';
		else if (ent == "quot") decoded = '"';
		else if (ent == "apos") decoded = '\'';
		else if (!ent.empty() && ent[0] == '#')
		{
			String num = ent.substr(1);
			char* end = nullptr;
			long v;
			if (!num.empty() && (num[0] == 'x' || num[0] == 'X'))
				v = std::strtol(num.c_str() + 1, &end, 16);
			else
				v = std::strtol(num.c_str(), &end, 10);
			if (end && *end == '\0' && v > 0 && v <= 0x10FFFF)
				out += (char)v;   // best-effort for the BMP/ASCII range
			else
				out += '&' + ent + ';';
			i = semi;
			continue;
		}
		if (decoded)
		{
			out += decoded;
			i = semi;
		}
		else
		{
			if (!m_warned_unknown_entity)
			{
				Error(line, "unknown entity '&" + ent + ";' kept as-is");
				m_warned_unknown_entity = true;
			}
			out += raw.substr(i, semi - i + 1);
			i = semi;
		}
	}
	return out;
}

void RmlParser::Error(int line, const String& message)
{
	m_doc.errors.push_back(UIError{ line, message });
}

MYRENDERER_END_NAMESPACE // UIDocModel
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
