#pragma once
#ifndef _UIDOCUMENT_SERIALIZER_
#define _UIDOCUMENT_SERIALIZER_

#include "UIDocumentModel.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(UIDocModel)

/// Deterministic round-trip-stable serialization:
///   parse → serialize → parse → serialize is byte-identical (second pass).
/// RML formatting contract:
///   - one tab per depth, empty elements self-close, comments preserved
///   - text nodes are whitespace-collapsed and trimmed; whitespace-only
///     nodes between block elements are dropped
///   - head (title/link) and body attributes are re-emitted from the IR
class UIDocumentSerializer
{
public:
	static String SerializeRml(const UIDocument& doc);
	static String SerializeRcss(const UIStyleSheet& stylesheet);
};

MYRENDERER_END_NAMESPACE // UIDocModel
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UIDOCUMENT_SERIALIZER_
