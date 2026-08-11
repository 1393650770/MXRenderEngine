#include "UIDocumentModel.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(UIDocModel)

const String* UIDocumentNode::GetAttribute(const String& name) const
{
	for (const auto& attr : attributes)
		if (attr.name == name)
			return &attr.value;
	return nullptr;
}

String UIDocumentNode::GetAttributeOr(const String& name, const String& fallback) const
{
	const String* v = GetAttribute(name);
	return v ? *v : fallback;
}

void UIDocumentNode::SetAttribute(const String& name, const String& value)
{
	for (auto& attr : attributes)
	{
		if (attr.name == name)
		{
			attr.value = value;
			return;
		}
	}
	attributes.push_back(UIAttribute{ name, value });
}

String UIDocumentNode::GetId() const
{
	return GetAttributeOr("id", "");
}

UIDocumentNode* UIDocumentNode::FirstChildElement(const String& tag)
{
	for (auto& child : children)
		if (child->type == ENodeType::Element && child->tag_name == tag)
			return child.get();
	return nullptr;
}

MYRENDERER_END_NAMESPACE // UIDocModel
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender
