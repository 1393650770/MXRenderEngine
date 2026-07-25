#pragma once
#ifndef _RENDERBINDLESSMANAGER_
#define _RENDERBINDLESSMANAGER_

#include "Core/ConstDefine.h"
#include "Core/ResourceHandle.h"
#include "RenderResource.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)

class Texture;

// --  Bindless  --  --  Tag  --  -- （ --  --  struct  --  --  --  --  --  --）
//  --  --  --  ResourceHandle<Tag>  --  --   compile-time  --  --  --  --  --
struct TagBindlessTexture2D {};
struct TagBindlessTextureCube {};
struct TagBindlessSampler {};
using BindlessSlotHandle      = ResourceHandle<TagBindlessTexture2D>;
using BindlessCubeSlotHandle  = ResourceHandle<TagBindlessTextureCube>;
using BindlessSamplerHandle   = ResourceHandle<TagBindlessSampler>;

// --   Platform-agnostic bindless resource manager interface
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(BindlessManager, public RenderResource)
#pragma region METHOD
public:
	VIRTUAL ~BindlessManager() MYDEFAULT;
	VIRTUAL BindlessSlotHandle     METHOD(AllocateTexture2DSlot)(Texture* texture) PURE;
	VIRTUAL void                   METHOD(FreeTexture2DSlot)(BindlessSlotHandle handle) PURE;
	VIRTUAL BindlessCubeSlotHandle METHOD(AllocateTextureCubeSlot)(Texture* texture) PURE;
	VIRTUAL void                   METHOD(FreeTextureCubeSlot)(BindlessCubeSlotHandle handle) PURE;
	VIRTUAL Bool                   METHOD(IsEnabled)() CONST PURE;
#pragma endregion

#pragma region MEMBER
public:
private:
protected:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif
