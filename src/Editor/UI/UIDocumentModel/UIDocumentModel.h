#pragma once
#ifndef _UIDOCUMENT_MODEL_
#define _UIDOCUMENT_MODEL_

#include "Core/ConstDefine.h"
#include <memory>
#include <vector>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(UIDocModel)

/// Pure-data IR for RML documents + RCSS stylesheets. Backend-agnostic (no
/// RmlUi types) — the editor's UI designer works on this, and serialization
/// is a stable round-trip (parse → serialize → parse is byte-identical).
/// Mirrors the RenderGraphDefinition pattern but lives Editor-side only.

struct UIError
{
	int line = 0;
	String message;
};

// ---------------------------------------------------------------------------
// RML side
// ---------------------------------------------------------------------------

enum class ENodeType
{
	Element,   // <tag attrs>...</tag> (or self-closing)
	Text,      // raw text content
	Comment,   // <!-- ... -->
};

struct UIAttribute
{
	String name;    // as authored (case preserved)
	String value;
};

struct UIDocumentNode
{
	ENodeType type = ENodeType::Element;
	String tag_name;              // Element
	String text;                  // Text / Comment content
	Vector<UIAttribute> attributes;
	Vector<std::unique_ptr<UIDocumentNode>> children;
	UIDocumentNode* parent = nullptr;
	int source_line = 0;

	const String* GetAttribute(const String& name) const;
	String GetAttributeOr(const String& name, const String& fallback) const;
	void SetAttribute(const String& name, const String& value);
	String GetId() const;
	/// First Element child matching tag (nullptr if none).
	UIDocumentNode* FirstChildElement(const String& tag);
};

struct UIDocument
{
	String file_path;              // relative, e.g. "RmlUI/__preview__.rml"
	String title;                  // <head><title>
	Vector<String> stylesheets;    // <head><link href=...>
	Vector<UIAttribute> body_attributes;   // <body data-model=... id=...> — preserved for round-trip
	Vector<std::unique_ptr<UIDocumentNode>> nodes;   // top-level children of <body>
	Vector<UIError> errors;
};

// ---------------------------------------------------------------------------
// RCSS side
// ---------------------------------------------------------------------------

struct UICssProperty
{
	String name;      // lowercased
	String value;     // canonical single-space-joined tokens
	int source_line = 0;
};

struct UIRuleSet
{
	Vector<String> selectors;      // split on ',' (as authored, trimmed)
	Vector<UICssProperty> properties;
	int source_line = 0;
};

enum class EAtRuleKind
{
	Media,        // @media <cond> { rules }
	Keyframes,    // @keyframes <name> { keyframe rules }
	FontFace,     // @font-face { declarations }
	Decorator,    // @decorator ... { declarations }
	SpriteSheet,  // @spritesheet { declarations }
	Unknown,
};

struct UIAtRule
{
	EAtRuleKind kind = EAtRuleKind::Unknown;
	String name;                  // lowercased, no '@'
	String prelude;               // between name and '{'
	Vector<UIRuleSet> rules;      // Media / Keyframes bodies
	Vector<UICssProperty> properties;  // FontFace / Decorator / SpriteSheet
	int source_line = 0;
};

struct UIStyleSheet
{
	String file_path;              // relative, e.g. "RmlUI/__preview__.rcss"
	Vector<UIRuleSet> rules;
	Vector<UIAtRule> at_rules;
	Vector<UIError> errors;
};

MYRENDERER_END_NAMESPACE // UIDocModel
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_UIDOCUMENT_MODEL_
