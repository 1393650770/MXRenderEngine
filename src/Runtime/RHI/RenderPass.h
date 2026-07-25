#pragma once
#ifndef _RENDERPASS_
#define _RENDERPASS_
#include "RenderResource.h"



MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)

MYRENDERER_BEGIN_STRUCT(RenderPassCacheKeyHash)
public:
	UInt64 operator()(CONST RenderPassCacheKey& key) CONST
	{
		return key.GetHash();
	}
MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RenderPass, public RenderResource)

#pragma region METHOD
public:
	RenderPass(CONST RenderPassDesc& in_desc);
	VIRTUAL ~RenderPass() MYDEFAULT;
protected:
private:

#pragma endregion

#pragma region MEMBER
public:
protected:

	RenderPassDesc desc;
private:

#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE


#endif
