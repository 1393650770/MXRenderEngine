#pragma once
#ifndef _BUFFER_
#define _BUFFER_
#include "RenderEnum.h"
#include "RenderResource.h"
MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(Buffer,  public RenderResource)
#pragma region METHOD
public:
	Buffer(CONST BufferDesc& in_buffer_desc) ;
	VIRTUAL ~Buffer() MYDEFAULT;
	VIRTUAL void* METHOD(Map)(CONST ENUM_MAP_TYPE& map_type, CONST ENUM_MAP_FLAG& map_flag) PURE ;
	VIRTUAL void METHOD(Unmap)() PURE ;
	VIRTUAL BufferDesc METHOD(GetBufferDesc)() CONST;
protected:

private:

#pragma endregion


#pragma region MEMBER
public:

protected:
	BufferDesc buffer_desc;
private:
#pragma endregion



MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif
