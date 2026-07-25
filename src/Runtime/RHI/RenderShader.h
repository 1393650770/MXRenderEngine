#pragma once
#ifndef _SHADER_
#define _SHADER_

#include "RenderEnum.h"
#include "Core/ConstDefine.h"
#include "RenderResource.h"


MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)

MYRENDERER_BEGIN_STRUCT( ReflectionOverrides) 
	CONST Char* name;
	ENUM_BINDING_RESOURCE_TYPE override_type;
MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_CLASS(ShaderResourceBinding)

#pragma region METHOD
public:
	ShaderResourceBinding() MYDEFAULT;

	VIRTUAL ~ShaderResourceBinding();

	VIRTUAL void METHOD(SetResource)(CONST String& name, CONST RenderResource* resource) PURE;
	// --  
	VIRTUAL void METHOD(FlushDescriptorWrites)() { /* default no-op */ }
protected:
private:

#pragma endregion

#pragma region MEMBER
public:
protected:
	ENUM_SHADER_RESOURCE_BINDING_POINT_TYPE type = ENUM_SHADER_RESOURCE_BINDING_POINT_TYPE::Invalid;
private:

#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(Shader, public RenderResource)

#pragma region METHOD
public:
	Shader() MYDEFAULT;
	Shader(CONST ShaderDesc& desc);
	Shader(CONST ShaderDesc& desc,CONST ShaderDataPayload& data);
	VIRTUAL ~Shader();

	CONST ShaderDesc& GetDesc() CONST;
	constexpr Bool operator == (CONST Shader& rhs) CONST
	{
		return this->GetDesc().debug_name == rhs.GetDesc().debug_name &&
			this->GetDesc().entry_name == rhs.GetDesc().entry_name &&
			this->GetDesc().shader_name == rhs.GetDesc().shader_name &&
			this->GetDesc().shader_type == rhs.GetDesc().shader_type;
	};

	constexpr Bool operator != (CONST Shader& rhs) CONST
	{
		return !(*this == rhs);
	};

protected:
private:

#pragma endregion

#pragma region MEMBER
public:
protected:
	ShaderDesc desc;
private:

#pragma endregion
    
MYRENDERER_END_CLASS



MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif //_SHADER_
