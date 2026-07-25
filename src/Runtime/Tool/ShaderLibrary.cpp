#include "ShaderLibrary.h"
#include <fstream>
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderShader.h"
#include "RHI/RenderPipelineState.h"
#if PLATFORM_ANDROID
#include <android/asset_manager.h>
#endif

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Tool)

#if PLATFORM_ANDROID
AAssetManager* ShaderLibrary::s_asset_manager = nullptr;

void ShaderLibrary::SetAssetManager(AAssetManager* in_mgr)
{
	s_asset_manager = in_mgr;
}
#endif

Vector<UInt32> ShaderLibrary::ReadSpirv(CONST String& in_filename)
{
#if PLATFORM_ANDROID
	if (s_asset_manager)
	{
		AAsset* asset = AAssetManager_open(s_asset_manager, in_filename.c_str(), AASSET_MODE_BUFFER);
		if (asset)
		{
			size_t file_size = (size_t)AAsset_getLength(asset);
			Vector<UInt32> buffer(file_size / sizeof(UInt32));
			AAsset_read(asset, buffer.data(), file_size);
			AAsset_close(asset);
			return buffer;
		}
	}
#endif
	std::ifstream file(in_filename, std::ios::ate | std::ios::binary);
	CHECK_WITH_LOG(!file.is_open(), ("Tool Error: fail to open the shader file: " + in_filename).c_str())
	size_t file_size = (size_t)file.tellg();
	Vector<UInt32> buffer(file_size / sizeof(UInt32));
	file.seekg(0);
	file.read((char*)buffer.data(), file_size);
	file.close();
	return buffer;
}

#if PLATFORM_WGPU
String ShaderLibrary::ReadWgsl(CONST String& in_filename)
{
	std::ifstream file(in_filename, std::ios::ate);
	CHECK_WITH_LOG(!file.is_open(), ("Tool Error: fail to open the WGSL shader file: " + in_filename).c_str())
	size_t file_size = (size_t)file.tellg();
	String source(file_size, '\0');
	file.seekg(0);
	file.read(&source[0], file_size);
	file.close();
	return source;
}
#endif

RHI::Shader* ShaderLibrary::LoadShader(ENUM_SHADER_STAGE in_stage, CONST String& in_filename)
{
	RHI::ShaderDesc desc;
	desc.shader_type = in_stage;
	desc.shader_name = in_filename;
	desc.entry_name = "main";
	RHI::ShaderDataPayload payload;
#if PLATFORM_WGPU
	// WebGPU path: load .wgsl text (filename convention: "Shader/xxx.vert.spv" → "Shader/xxx.vert.wgsl")
	// The build pipeline (tint) converts GLSL→SPIR-V→WGSL at compile time.
	String wgsl_path = in_filename;
	const String spv_ext = ".spv";
	if (wgsl_path.size() >= spv_ext.size() &&
		wgsl_path.compare(wgsl_path.size() - spv_ext.size(), spv_ext.size(), spv_ext) == 0)
	{
		wgsl_path.replace(wgsl_path.size() - spv_ext.size(), spv_ext.size(), ".wgsl");
	}
	payload.wgsl_source = ReadWgsl(wgsl_path);
#else
	payload.data = ReadSpirv(in_filename);
#endif
	return RHICreateShader(desc, payload);
}

RHI::RenderPipelineState* ShaderLibrary::CreateComputePSO(RHI::Shader* in_compute_shader)
{
	RHI::RenderGraphiPipelineStateDesc desc{};
	desc.shaders[ENUM_SHADER_STAGE::Shader_Compute] = in_compute_shader;
	desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
	desc.raster_state.sample_count = 1;
	return g_render_rhi->CreateRenderPipelineState(desc);
}

RHI::RenderPipelineState* ShaderLibrary::CreateComputePSOFromFile(CONST String& in_filename)
{
	RHI::Shader* compute_shader = LoadShader(ENUM_SHADER_STAGE::Shader_Compute, in_filename);
	RHI::RenderPipelineState* pso = CreateComputePSO(compute_shader);
	delete compute_shader;
	return pso;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
