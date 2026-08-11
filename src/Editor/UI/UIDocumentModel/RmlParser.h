#pragma once
#ifndef _RML_PARSER_
#define _RML_PARSER_

#include "UIDocumentModel.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(UIDocModel)

/// Tolerant XML-subset parser for RML documents. Never aborts — every
/// malformed construct becomes a UIError with a line number:
///  - unclosed tags auto-close at EOF, mismatched close tags are skipped,
///    duplicate attributes last-wins, bare '<' becomes text
///  - `<?...?>` / `<!DOCTYPE ...>` skipped; `<!-- -->` kept as comments
///  - entities: &amp; &lt; &gt; &quot; &apos; &#NN; &#xHH; (others literal)
///  - `{{ expr }}` and `data-*` expressions stay opaque
class RmlParser
{
public:
	static UIDocument Parse(const String& text, const String& file_path);

private:
	RmlParser(const String& text, const String& file_path);

	void ParseContainer(const String& container_tag, bool is_head);
	bool ConsumeTagOpen(UIDocumentNode* node, bool& out_self_closing);
	void SkipElement(const String& tag);
	void AdvanceTo(size_t pos);
	String ParseText();
	String DecodeEntities(const String& raw, int line);
	void Error(int line, const String& message);

	const String& m_text;
	String m_file_path;
	size_t m_pos = 0;
	int m_line = 1;
	UIDocument m_doc;
	bool m_warned_unknown_entity = false;
};

MYRENDERER_END_NAMESPACE // UIDocModel
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_RML_PARSER_
