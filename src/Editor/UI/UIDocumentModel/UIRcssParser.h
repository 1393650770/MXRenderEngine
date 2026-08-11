#pragma once
#ifndef _UI_RCSS_PARSER_
#define _UI_RCSS_PARSER_

#include "UIDocumentModel.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(UIDocModel)

/// Parses an RCSS file into UIStyleSheet by wrapping CssToRcssLib's parser
/// (RCSS is by construction a valid CSS subset — parsing only, no conversion).
class UIRcssParser
{
public:
	static UIStyleSheet Parse(const String& text, const String& file_path);
};

MYRENDERER_END_NAMESPACE // UIDocModel
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UI_RCSS_PARSER_
