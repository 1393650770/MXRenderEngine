#include "Render/Core/RenderGraphBuilder.h"
#include <fstream>
#include <cstring>
#include "RHI/RenderShader.h"
#include "RHI/RenderPipelineState.h"
#include "Render/Core/RenderGraphDefinition.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphResource.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderEnum.h"
#include "RHI/RenderTexture.h"
#include "RHI/RenderBuffer.h"
#include "RHI/RenderCommandList.h"
#include "Asset/TextureAsset.h"
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)

String RenderGraphBuilder::s_last_error;

// Global pass execution registry
static Map<String, PassExecuteFunc> g_pass_execute_registry;

void RenderGraphBuilder::RegisterPassExecute(CONST String& pass_name, PassExecuteFunc func)
{
	g_pass_execute_registry[pass_name] = std::move(func);
}

struct MinimalPassData : public RenderGraphPassDataBase
{
	PassExecuteFunc exec_callback;
	// --  Generic shader rendering
	String vs_path, ps_path;
	RHI::RenderPipelineState* pipeline = nullptr;
	RHI::ShaderResourceBinding* srb = nullptr;
	UInt32 vertex_count = 3; // -- 
	void Release() override {
		if (srb) { delete srb; srb = nullptr; }
		// pipeline is owned by VK_PipelineStateManager — do NOT delete
	}
};

Bool RenderGraphBuilder::BuildRuntimeGraph(
	CONST RenderGraphDefinition& def,
	RenderGraph* out_graph,
	CONST Vector<ExternalResourceBinding>& externals)
{
	s_last_error.clear();
	if (!out_graph) { s_last_error = "BuildRuntimeGraph: out_graph is null"; return false; }
	out_graph->Release();

	// ---- Phase 1: Register resources ----
	Map<String, RenderGraphResourceBase*> resource_map;

	// External bindings first (swapchain BackBuffer / DepthStencil)
	for (auto& ext : externals)
	{
		if (ext.texture) {
			auto* tex = static_cast<RHI::Texture*>(ext.texture);
			RHI::TextureDesc desc = tex->GetTextureDesc();
			resource_map[ext.resource_name] = out_graph->AddRetainedResource<RHI::TextureDesc, RHI::Texture>(
				ext.resource_name, desc, tex);
		} else if (ext.buffer) {
			auto* buf = static_cast<RHI::Buffer*>(ext.buffer);
			RHI::BufferDesc desc = buf->GetBufferDesc();
			resource_map[ext.resource_name] = out_graph->AddRetainedResource<RHI::BufferDesc, RHI::Buffer>(
				ext.resource_name, desc, buf);
		}
	}

	// Defined resources (skip if already bound externally)
	for (auto& rd : def.resources)
	{
		if (resource_map.count(rd.name)) continue;
		if (rd.lifetime == RDGResourceLifetime::External || !rd.is_transient)
		{
			if (std::holds_alternative<RHI::BufferDesc>(rd.desc))
			{
				auto desc = std::get<RHI::BufferDesc>(rd.desc);
				if (!rd.buffer_data.empty()) {
					// --   Create GPU buffer from serialized data
					RHI::Buffer* buf = g_render_rhi->CreateBuffer(desc);
					void* p = RHIMapBuffer(buf, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::None);
					memcpy(p, rd.buffer_data.data(), rd.buffer_data.size());
					RHIUnmapBuffer(buf);
					resource_map[rd.name] = out_graph->AddRetainedResource<RHI::BufferDesc, RHI::Buffer>(rd.name, desc, buf);
				} else {
					resource_map[rd.name] = out_graph->AddRetainedResource<RHI::BufferDesc, RHI::Buffer>(rd.name, desc, nullptr);
				}
			}
			else if (std::holds_alternative<RHI::ShaderDesc>(rd.desc))
			{
				auto desc = std::get<RHI::ShaderDesc>(rd.desc);
				resource_map[rd.name] = out_graph->AddRetainedResource<RHI::ShaderDesc, RHI::Shader>(rd.name, desc, nullptr);
			}
			else
			{
				auto desc = std::get<RHI::TextureDesc>(rd.desc);
				// --  Load from file if path specified
				RHI::Texture* existing_tex = nullptr;
				if (!rd.file_path.empty()) {
					auto* asset = new Asset::TextureAsset(rd.file_path);
					while (!asset->GetTexture()) {} // Wait for async load
					existing_tex = asset->GetTexture();
					resource_map[rd.name] = out_graph->AddRetainedResource<RHI::TextureDesc, RHI::Texture>(rd.name, desc, existing_tex);
				} else {
					resource_map[rd.name] = out_graph->AddRetainedResource<RHI::TextureDesc, RHI::Texture>(rd.name, desc, nullptr); // --   placeholder
				}
			}
		}
	}

	// ---- Phase 2: Build passes ----
	UInt32 built_count = 0;
	for (auto& pd : def.passes)
	{
		String pass_name = pd.name;

		// Look up registered pass execution callback
		PassExecuteFunc exec_cb = nullptr;
		auto exec_it = g_pass_execute_registry.find(pass_name);
		if (exec_it != g_pass_execute_registry.end())
			exec_cb = exec_it->second;
		else {
			auto wildcard_it = g_pass_execute_registry.find("");
			if (wildcard_it != g_pass_execute_registry.end())
				exec_cb = wildcard_it->second;
		}

		// Capture copies for execute lambda
		Vector<String> write_names = pd.write_resources;
		Map<String, RenderGraphResourceBase*> exec_res_map = resource_map;

		auto* pass = out_graph->AddRenderPass<MinimalPassData>(
			pass_name,
			[&, exec_cb](MinimalPassData& data, RenderGraphPassBuilder& builder, RHI::CommandList*)
			{
				data.exec_callback = exec_cb;
				for (auto& rn : pd.read_resources) {
					auto it = resource_map.find(rn);
					if (it != resource_map.end()) builder.Read(it->second);
				}
				for (auto& rn : pd.write_resources) {
					auto it = resource_map.find(rn);
					if (it != resource_map.end()) builder.Write(it->second);
				}
				// --   If no registered callback but shader_path is set, create a generic pipeline
				if (!exec_cb && !pd.shader_path.empty()) {
					data.vertex_count = pd.vertex_count;
					String vs_file = pd.shader_path + ".vert.spv";
					String ps_file = pd.shader_path + ".frag.spv";
					std::ifstream vs_f(vs_file, std::ios::ate | std::ios::binary);
					std::ifstream ps_f(ps_file, std::ios::ate | std::ios::binary);
					if (vs_f.is_open() && ps_f.is_open()) {
						size_t vs_sz = (size_t)vs_f.tellg(); Vector<UInt32> vs_buf(vs_sz / sizeof(UInt32));
						vs_f.seekg(0); vs_f.read((char*)vs_buf.data(), vs_sz); vs_f.close();
						size_t ps_sz = (size_t)ps_f.tellg(); Vector<UInt32> ps_buf(ps_sz / sizeof(UInt32));
						ps_f.seekg(0); ps_f.read((char*)ps_buf.data(), ps_sz); ps_f.close();
						RHI::ShaderDesc vs_d; vs_d.shader_type = ENUM_SHADER_STAGE::Shader_Vertex; vs_d.shader_name = pd.name + "_VS";
						RHI::ShaderDataPayload vs_payload; vs_payload.data = vs_buf;
						RHI::Shader* vs = RHICreateShader(vs_d, vs_payload);
						RHI::ShaderDesc ps_d; ps_d.shader_type = ENUM_SHADER_STAGE::Shader_Pixel; ps_d.shader_name = pd.name + "_PS";
						RHI::ShaderDataPayload ps_payload; ps_payload.data = ps_buf;
						RHI::Shader* ps = RHICreateShader(ps_d, ps_payload);
						RHI::RenderGraphiPipelineStateDesc ps_desc;
						ps_desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
						ps_desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps;
						ps_desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
						ps_desc.raster_state.sample_count = 1;
						// Get render target format from BackBuffer
						auto bb_it = resource_map.find("BackBuffer");
						if (bb_it != resource_map.end() && bb_it->second->GetAsTexture()) {
							ps_desc.render_targets = { bb_it->second->GetAsTexture() };
						}
						// Get depth stencil if available
						auto ds_it = resource_map.find("DepthStencil");
						if (ds_it != resource_map.end() && ds_it->second->GetAsTexture())
							ps_desc.depth_stencil_view = ds_it->second->GetAsTexture();
						ps_desc.blend_state.render_targets.resize(1);
						data.pipeline = g_render_rhi->CreateRenderPipelineState(ps_desc);
						data.pipeline->CreateShaderResourceBinding(data.srb);
						delete vs; delete ps;
					} else {
						std::cerr << "[RenderGraphBuilder] Failed to open shader: " << vs_file << " or " << ps_file << std::endl;
					}
				}
			},
			[write_names, exec_res_map](const MinimalPassData& data, RHI::CommandList* cmd)
			{
				if (data.exec_callback)
				{
					auto mutable_map = exec_res_map;
					data.exec_callback(cmd, mutable_map);
				}
				else if (data.pipeline)
				{
					// --   Generic shader-based pass rendering (from shader_path)
					auto bb_it = exec_res_map.find("BackBuffer");
					auto ds_it = exec_res_map.find("DepthStencil");
					RHI::Texture* rt_tex = (bb_it != exec_res_map.end() && bb_it->second) ? bb_it->second->GetAsTexture() : nullptr;
					RHI::Texture* ds_tex = (ds_it != exec_res_map.end() && ds_it->second) ? ds_it->second->GetAsTexture() : nullptr;
					if (rt_tex) {
						Vector<RHI::ClearValue> cvs = { RHI::ClearValue{0.0f, 0.0f, 0.0f, 1.0f} };
						if (ds_tex) cvs.push_back(RHI::ClearValue{1.0f, 0});
						cmd->SetRenderTarget({rt_tex}, ds_tex, cvs, ds_tex != nullptr);
						cmd->SetGraphicsPipeline(data.pipeline);
						cmd->SetShaderResourceBinding(data.srb);
						DrawAttribute draw_attr; draw_attr.vertexCount = data.vertex_count; draw_attr.instanceCount = 1;
						cmd->Draw(draw_attr);
					}
				}
				else
				{
					// Default execute: clear bound render targets
					for (auto& rn : write_names) {
						auto it = exec_res_map.find(rn);
						if (it == exec_res_map.end() || !it->second) continue;
						if (auto* tex = it->second->GetAsTexture()) {
							const auto& desc = tex->GetTextureDesc();
							bool is_depth = ((UInt32)desc.usage & 16) != 0; // ENUM_TYPE_DEPTH_ATTACHMENT = 1 << 4
							if (is_depth) {
								RHI::ClearValue cv{ 1.0f, 0 };
								cmd->SetRenderTarget({}, tex, {cv}, true);
								cmd->ClearTexture(tex);
							} else {
								Vector<RHI::Texture*> rtvs = { tex };
								RHI::ClearValue cv{ 0.15f, 0.15f, 0.25f, 1.0f };
								Vector<RHI::ClearValue> cvs = { cv };
								cmd->SetRenderTarget(rtvs, nullptr, cvs, false);
								cmd->ClearTexture(tex);
							}
						}
					}
				}
			}
		);

		if (static_cast<UInt32>(pd.pass_flags) & static_cast<UInt32>(RDGPassFlags::NeverCull))
			pass->SetIsCullable(false);
		built_count++;
	}

	std::cout << "[RenderGraphBuilder] Built graph: " << def.graph_name
		<< " (" << built_count << " passes, " << resource_map.size() << " resources)" << std::endl;
	return true;
}

void RenderGraphBuilder::GetBuildStats(
	CONST RenderGraphDefinition& def,
	UInt32& out_pass_count,
	UInt32& out_resource_count)
{
	out_pass_count = (UInt32)def.passes.size();
	out_resource_count = (UInt32)def.resources.size();
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
