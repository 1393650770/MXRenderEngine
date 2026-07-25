#pragma once
// -- [AI:BEGIN]
#ifndef _NN_SHADERHELPER_
#define _NN_SHADERHELPER_
#include <fstream>
#include "Core/ConstDefine.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderShader.h"
#include "RHI/RenderResource.h"
#include "RHI/ResourceManager.h"

namespace MXNN {

inline Vector<UInt32> ReadShader(CONST String& in_filename)
{
	std::ifstream file(in_filename, std::ios::ate | std::ios::binary);
	CHECK_WITH_LOG(!file.is_open(), " App Error: fail to open the shader file! ")
	size_t file_size = (size_t)file.tellg();
	Vector<UInt32> buffer(file_size / sizeof(UInt32));
	file.seekg(0);
	file.read((char*)buffer.data(), file_size);
	file.close();
	return std::move(buffer);
}

inline MXRender::RHI::Shader* LoadComputeShader(CONST String& in_filename)
{
	MXRender::RHI::ShaderDesc desc;
	desc.shader_type = MXRender::ENUM_SHADER_STAGE::Shader_Compute;
	desc.entry_name = "main";
	desc.shader_name = in_filename;
	MXRender::RHI::ShaderDataPayload payload;
	payload.data = ReadShader(in_filename);
	return RHICreateShader(desc, payload);
}

inline MXRender::RHI::RenderPipelineState* CreateComputePipeline(MXRender::RHI::Shader* in_cs)
{
	MXRender::RHI::RenderGraphiPipelineStateDesc desc{};
	desc.shaders[MXRender::ENUM_SHADER_STAGE::Shader_Compute] = in_cs;
	desc.primitive_topology = MXRender::ENUM_PRIMITIVE_TYPE::TriangleList;
	desc.raster_state.sample_count = 1;
	auto _handle = RHICreateRenderPipelineState(desc);
	return Resolve(_handle);
}

} // namespace MXNN
// -- [AI:END]
#endif // _NN_SHADERHELPER_
