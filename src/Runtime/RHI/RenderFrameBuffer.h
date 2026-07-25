#pragma once
#ifndef _FRAMEBUFFER_
#define _FRAMEBUFFER_
#include "RenderResource.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(FrameBuffer,public RenderResource)

#pragma region METHOD
    public:
        FrameBuffer() MYDEFAULT;
        FrameBuffer(CONST FrameBufferDesc& in_desc);
        virtual ~FrameBuffer() MYDEFAULT;
    protected:

    private:
#pragma endregion

#pragma region MEMBER
	public:

	protected:
        FrameBufferDesc desc;
	private:
#pragma endregion
        
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE


#endif
