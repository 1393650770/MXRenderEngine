#include "Render.h"

#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderShader.h"
#include "Asset/TextureAsset.h"
#include <iostream>
#include <fstream>
#include "Application/Window.h"
#include "RHI/RenderViewport.h"
#include "RHI/RenderTexture.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderBuffer.h"
#include "RHI/RenderUtils.h"
#include "UI/RenderGraphEditor/Panels/RenderGraphPanel.h"
#include "UI/RenderGraphEditor/Services/EditorEventBus.h"
#include "Render/Core/RenderGraphBuilder.h"
#include "Render/Core/RenderGraphDefinition.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Application)


Vector<UInt32> ReadShader(CONST String& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	CHECK_WITH_LOG(!file.is_open(), " App Error: fail to open the shader file! ")

		size_t fileSize = (size_t)file.tellg();
	Vector<UInt32> buffer(fileSize / sizeof(UInt32));

	file.seekg(0);
	file.read((char*)buffer.data(), fileSize);

	file.close();

	return std::move(buffer);
}

//  Logic thread: ImGui init, editor UI (GLFW-dependent)
void EditorRenderPipeline::OnInit_Logic(PlatformWindow* in_window, RHI::Viewport* in_viewport)
{
	m_window = in_window; m_viewport = in_viewport;
	std::cout << "Hello Editor" << std::endl;
	editor_ui.Init(m_window, m_viewport);
}

//  Render thread: shaders, pipelines, RenderGraph passes, compile
void EditorRenderPipeline::OnInit_Render()
{
	RHI::CommandList* cmd_list = RHIGetImmediateCommandList();

	// Register backbuffer as retained (imported) resources so the RDG knows about them.
	// This enables the editor to visualize pass-resource dependencies.
	RHI::Texture* backbuffer_rtv = m_viewport->GetCurrentBackBufferRTV();
	RHI::Texture* backbuffer_dsv = m_viewport->GetCurrentBackBufferDSV();

	RHI::TextureDesc rt_desc = backbuffer_rtv->GetTextureDesc();
	Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>* rt_resource =
		graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("BackBuffer", rt_desc, backbuffer_rtv);

	Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>* ds_resource = nullptr;
	if (backbuffer_dsv)
	{
		RHI::TextureDesc ds_desc = backbuffer_dsv->GetTextureDesc();
		ds_resource = graph.AddRetainedResource<RHI::TextureDesc, RHI::Texture>("DepthStencil", ds_desc, backbuffer_dsv);
	}

	struct ClearPassData :public  Render::RenderGraphPassDataBase
	{
		VIRTUAL ~ClearPassData()
		{
			Release();
		}
		void Release()
		{

		}
	};
	graph.AddRenderPass<ClearPassData>("ClearPass", &graph, cmd_list,
		[&](ClearPassData& data, Render::RenderGraphPassBuilder& builder, RHI::CommandList* in_cmd_list)
	{
		// Declare: ClearPass writes to BackBuffer and DepthStencil
		builder.Write(rt_resource);
		if (ds_resource) builder.Write(ds_resource);
	},
		[=](CONST ClearPassData& data, RHI::CommandList* in_cmd_list)
	{

		Vector<RHI::ClearValue> clear_values;
		Vector<RHI::Texture*> rtvs;
		RHI::Texture* dsv;
		rtvs = { this->m_viewport->GetCurrentBackBufferRTV() };
		dsv = this->m_viewport->GetCurrentBackBufferDSV();
		for (auto rtv : rtvs)
		{
			clear_values.push_back(rtv->GetTextureDesc().clear_value);
		}
		if (dsv)
			clear_values.push_back(dsv->GetTextureDesc().clear_value);
		in_cmd_list->SetRenderTarget(rtvs, dsv, clear_values, dsv != nullptr);
		for (auto rtv : rtvs)
		{
			in_cmd_list->ClearTexture(rtv);
		}
		if (dsv)
			in_cmd_list->ClearTexture(dsv);

	});


	struct TestData :public  Render::RenderGraphPassDataBase
	{

		RHI::RenderPipelineState* pipeline_state = nullptr;
		RHI::ShaderResourceBinding* srb = nullptr;
		Asset::TextureAsset* bind_texture = nullptr;
		RHI::Buffer* const_buffer = nullptr;
		VIRTUAL ~TestData()
		{
			Release();
		}
		void Release()
		{
			if (srb)
			{
				delete srb;
				srb = nullptr;
			}
			if (bind_texture)
			{
				delete bind_texture;
				bind_texture = nullptr;
			}
			if (const_buffer)
			{
				delete const_buffer;
				const_buffer = nullptr;
			}
		}
	};

	graph.AddRenderPass<TestData>("TestPass", &graph, cmd_list,
		[&](TestData& data, Render::RenderGraphPassBuilder& builder, RHI::CommandList* in_cmd_list)
	{
		// TestPass: reads depth for testing, writes to backbuffer and depth
		builder.Write(rt_resource);
		if (ds_resource) builder.Read(ds_resource);
		if (ds_resource) builder.Write(ds_resource);

		RHI::Shader* vs_shader;
		RHI::Shader* ps_shader;
		RHI::ShaderDesc shader_desc;
		RHI::ShaderDataPayload shader_data;
		RHI::RenderGraphiPipelineStateDesc pipeline_state_desc;
		RHI::TextureDesc texture_desc;
		RHI::TextureDataPayload texture_data;
		shader_desc.shader_type = ENUM_SHADER_STAGE::Shader_Vertex;
		shader_desc.shader_name = "TestVS";
		shader_data.data = ReadShader("Shader/texture_test.vert.spv");
		vs_shader = RHICreateShader(shader_desc, shader_data);

		shader_desc.shader_type = ENUM_SHADER_STAGE::Shader_Pixel;
		shader_desc.shader_name = "TestPS";
		shader_data.data = ReadShader("Shader/stipple_transparency_test.frag.spv");
		ps_shader = RHICreateShader(shader_desc, shader_data);

		pipeline_state_desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs_shader;
		pipeline_state_desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps_shader;
		pipeline_state_desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		Vector<RHI::Texture*> rtvs;
		RHI::Texture* dsv;
		rtvs = { this->m_viewport->GetCurrentBackBufferRTV() };
		dsv = this->m_viewport->GetCurrentBackBufferDSV();
		pipeline_state_desc.render_targets = rtvs;
		pipeline_state_desc.depth_stencil_view = dsv;
		pipeline_state_desc.raster_state.sample_count = 1;
		pipeline_state_desc.blend_state.render_targets.resize(rtvs.size());

		data.pipeline_state = g_render_rhi->CreateRenderPipelineState(pipeline_state_desc);
		data.pipeline_state->CreateShaderResourceBinding(data.srb, true);

		data.bind_texture = new Asset::TextureAsset("Texture/pbr_stone/pbr_stone_aorm.dds");
		delete vs_shader;
		delete ps_shader;
		RHI::BufferDesc bufferdesc;
		bufferdesc.size = sizeof(float);
		bufferdesc.stride = sizeof(float);
		bufferdesc.type = ENUM_BUFFER_TYPE::Uniform;
		data.const_buffer = g_render_rhi->CreateBuffer(bufferdesc);
		data.srb->SetResource("constants", data.const_buffer);
	},
		[=](CONST TestData& data, RHI::CommandList* in_cmd_list)
	{
		if (data.bind_texture->GetTexture())
		{
			Vector<RHI::ClearValue> clear_values;
			Vector<RHI::Texture*> rtvs;
			RHI::Texture* dsv;
			rtvs = { this->m_viewport->GetCurrentBackBufferRTV() };
			dsv = this->m_viewport->GetCurrentBackBufferDSV();
			for (auto rtv : rtvs)
			{
				clear_values.push_back(rtv->GetTextureDesc().clear_value);
			}
			if (dsv)
				clear_values.push_back(dsv->GetTextureDesc().clear_value);
			in_cmd_list->SetRenderTarget(rtvs, dsv, clear_values, dsv != nullptr);
			in_cmd_list->SetGraphicsPipeline(data.pipeline_state);
			data.srb->SetResource("basecolor_sampler", data.bind_texture->GetTexture());

			struct Constant
			{
				float z = 0.0f;
			};
			{
				RHI::MapHelper<Constant> map_helper(data.const_buffer, ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::Discard);
				map_helper->z = 1.0f;
			}

			in_cmd_list->SetShaderResourceBinding(data.srb);
			DrawAttribute draw_attr;
			draw_attr.vertexCount = 6;
			draw_attr.instanceCount = 1;
			in_cmd_list->Draw(draw_attr);
		}
	});

	editor_ui.SetRenderGraph(&graph);
	graph.Compile();
	// Sync runtime graph to editor visualization
	if (auto* rgp = editor_ui.GetRenderGraphPanel())
		rgp->SyncRuntimeToEditor(&graph);

	// --  Register fallback shaders for loaded graph passes
	InitRenderPasses();
}

// -- 
void EditorRenderPipeline::InitRenderPasses()
{
	// ---- Skybox pipeline ----
	{
		RHI::ShaderDesc vs_desc; vs_desc.shader_type = ENUM_SHADER_STAGE::Shader_Vertex; vs_desc.shader_name = "SkyboxVS";
		RHI::ShaderDataPayload vs_data; vs_data.data = ReadShader("Shader/skybox_test.vert.spv");
		skybox_vs = RHICreateShader(vs_desc, vs_data);
		RHI::ShaderDesc ps_desc; ps_desc.shader_type = ENUM_SHADER_STAGE::Shader_Pixel; ps_desc.shader_name = "SkyboxPS";
		RHI::ShaderDataPayload ps_data; ps_data.data = ReadShader("Shader/skybox_test.frag.spv");
		skybox_ps = RHICreateShader(ps_desc, ps_data);
		RHI::RenderGraphiPipelineStateDesc pd; pd.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = skybox_vs; pd.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = skybox_ps;
		pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		pd.render_targets = { m_viewport->GetCurrentBackBufferRTV() };
		pd.depth_stencil_view = m_viewport->GetCurrentBackBufferDSV();
		pd.raster_state.sample_count = 1; pd.blend_state.render_targets.resize(1);
		skybox_pipeline = g_render_rhi->CreateRenderPipelineState(pd);
		skybox_pipeline->CreateShaderResourceBinding(skybox_srb, true);
	}
	Render::RenderGraphBuilder::RegisterPassExecute("SkyboxPass",
		[this](RHI::CommandList* cmd, Map<String, Render::RenderGraphResourceBase*>& res) {
			//   Get current swapchain image each frame (swapchain rotates images)
			auto* rt_tex = this->m_viewport->GetCurrentBackBufferRTV();
			auto* ds_tex = this->m_viewport->GetCurrentBackBufferDSV();
			std::cout << "[SkyboxPass] rt=" << (void*)rt_tex << " ds=" << (void*)ds_tex << " res_count=" << res.size() << std::endl;
			if (!rt_tex) { std::cout << "[SkyboxPass] no rt_tex!" << std::endl; return; }
			Vector<RHI::ClearValue> cvs = { RHI::ClearValue{0.2f,0.2f,0.3f,1.0f} };
			if (ds_tex) cvs.push_back(RHI::ClearValue{1.0f,0});
			cmd->SetRenderTarget({rt_tex}, ds_tex, cvs, ds_tex != nullptr);
			if (auto* cm = res["SkyboxCubemap"]) {
				if (auto* t = cm->GetAsTexture()) { skybox_srb->SetResource("cubemap_sampler", t); std::cout << "[SkyboxPass] cubemap bound" << std::endl; }
				else std::cout << "[SkyboxPass] cubemap GetAsTexture null" << std::endl;
			} else std::cout << "[SkyboxPass] no SkyboxCubemap in res" << std::endl;
			cmd->SetGraphicsPipeline(skybox_pipeline); cmd->SetShaderResourceBinding(skybox_srb);
			cmd->Draw(DrawAttribute{6,1,0,0});
			std::cout << "[SkyboxPass] draw done" << std::endl;
		});
	Render::RenderGraphBuilder::RegisterPassExecute("PBRPass",
		[this](RHI::CommandList* cmd, Map<String, Render::RenderGraphResourceBase*>& res) {
			//   Get current swapchain image each frame (swapchain rotates images)
			auto* rt_tex = this->m_viewport->GetCurrentBackBufferRTV();
			auto* ds_tex = this->m_viewport->GetCurrentBackBufferDSV();
			if (!rt_tex) return;
			cmd->SetRenderTarget({rt_tex}, ds_tex, {}, ds_tex != nullptr);
			cmd->SetGraphicsPipeline(skybox_pipeline); cmd->SetShaderResourceBinding(skybox_srb);
			cmd->Draw(DrawAttribute{6,1,0,0});
		});
}



void EditorRenderPipeline::RebuildFromDefinition(CONST MXRender::Render::RenderGraphDefinition& def)
{
	if (!m_window) return;
	Vector<Render::ExternalResourceBinding> externals;
	RHI::Texture* backbuffer_rtv = m_viewport->GetCurrentBackBufferRTV();
	RHI::Texture* backbuffer_dsv = m_viewport->GetCurrentBackBufferDSV();
	externals.push_back({"BackBuffer", backbuffer_rtv, nullptr});
	if (backbuffer_dsv) externals.push_back({"DepthStencil", backbuffer_dsv, nullptr});
	if (Render::RenderGraphBuilder::BuildRuntimeGraph(def, &graph, externals))
	{
		// --   Add ClearPass to ensure fresh framebuffer on graph switch
		auto* bb = graph.GetRetainedResource<RHI::TextureDesc, RHI::Texture>("BackBuffer");
		auto* ds = graph.GetRetainedResource<RHI::TextureDesc, RHI::Texture>("DepthStencil");
		struct ClearData : public Render::RenderGraphPassDataBase {
			void Release() override {}
		};
		graph.AddRenderPass<ClearData>("ClearPass", &graph, RHIGetImmediateCommandList(),
			[bb, ds](ClearData&, Render::RenderGraphPassBuilder& builder, RHI::CommandList*) {
				if (bb) builder.Write(bb);
				if (ds) builder.Write(ds);
			},
			[this](CONST ClearData&, RHI::CommandList* cmd) {
				auto* rt = this->m_viewport->GetCurrentBackBufferRTV();
				auto* dsv = this->m_viewport->GetCurrentBackBufferDSV();
				Vector<RHI::ClearValue> cvs = { rt->GetTextureDesc().clear_value };
				if (dsv) cvs.push_back(dsv->GetTextureDesc().clear_value);
				cmd->SetRenderTarget({rt}, dsv, cvs, dsv != nullptr);
			});
		// Move ClearPass to front so it executes before loaded passes.
		// Topological sort preserves insertion order for passes that only
		// write retained resources (no create_pass edges).
		auto& passes = graph.GetPasses();
		if (passes.size() >= 2)
		{
			for (size_t i = passes.size() - 1; i > 0; --i)
			{
				if (passes[i]->GetName() == "ClearPass")
				{
					auto clear_pass = std::move(passes[i]);
					passes.erase(passes.begin() + i);
					passes.insert(passes.begin(), std::move(clear_pass));
					break;
				}
			}
		}

		graph.Compile();
		if (auto* rgp = editor_ui.GetRenderGraphPanel()) rgp->SyncRuntimeToEditor(&graph);
	}
}

//  Logic thread: transfer deferred rebuild + ImGui context into FrameContext
// The definition is copied into a heap allocation owned by ctx, so Render thread
// can access it without touching the shared deferred_def.
void EditorRenderPipeline::OnPrepareFrameContext(Render::FrameContext& ctx)
{
	{
		std::lock_guard<std::mutex> lock(rebuild_mutex_);
		if (has_deferred_rebuild)
		{
			delete ctx.rebuild_def;
			ctx.rebuild_def = new Render::RenderGraphDefinition(deferred_def);
			ctx.has_rebuild = true;
			has_deferred_rebuild = false;
		}
	}
	// ImGui CPU work (NewFrame + widgets + Render) — must be on same thread as glfwPollEvents
	ctx.draw_data = editor_ui.DrawFrame_Logic();
	ctx.imgui_context = ImGui::GetCurrentContext();
}

//  Logic thread: release ImGui, editor UI (GLFW-dependent)
void EditorRenderPipeline::OnShutdown_Logic()
{
	editor_ui.Release();
}

//  Render thread: release GPU resources
void EditorRenderPipeline::OnShutdown_Render()
{
	if (skybox_srb) { delete skybox_srb; skybox_srb = nullptr; }
	if (skybox_pipeline) { delete skybox_pipeline; skybox_pipeline = nullptr; }
	if (skybox_vs) { delete skybox_vs; skybox_vs = nullptr; }
	if (skybox_ps) { delete skybox_ps; skybox_ps = nullptr; }

	graph.Release();
}

void EditorRenderPipeline::OnUpdate(float dt)
{
	//   Process editor command queue — execute all commands collected during the
	// previous frame's ImGui Draw(). This separates logic (OnUpdate) from rendering (OnRender).
	auto* rgp = editor_ui.GetRenderGraphPanel();
	if (rgp)
	{
		rgp->GetCommandQueue().ProcessAll(rgp->GetCommandHistory());
	}
			std::cout << "[OnUpdate] ProcessAll done, pending=" << (rgp->HasPendingBuild()?"YES":"NO") << std::endl;

	//   Tick debounced graph-modified event
	{
		std::lock_guard<std::mutex> lock(rebuild_mutex_);
		if (rgp->HasPendingBuild()) {
			deferred_def = rgp->GetPendingBuildDef(); has_deferred_rebuild = true; rgp->ClearPendingBuild();
			std::cout << "[OnUpdate] rebuild set, graph=" << deferred_def.graph_name << std::endl;
		}
	}
		UI::EditorEventBus::Get().TickFireGraphModified();
}

void EditorRenderPipeline::OnRender()
{
	static int fc = 0;
	if (++fc <= 5) std::cout << "[OnRender] frame=" << fc << " passes=" << graph.GetPasses().size() << std::endl;
	// Backbuffer update is done in OnPreRender (called before OnRender each frame)
	graph.Execute();
}

//  Render thread: pre-render — rebuild graph, update backbuffer, restore ImGui context
void EditorRenderPipeline::OnPreRender(Render::FrameContext& ctx)
{
	// Restore ImGui context (Logic saved it via OnPrepareFrameContext)
	// Deferred rebuild: definition was copied into ctx by OnPrepareFrameContext.
	if (ctx.has_rebuild && ctx.rebuild_def)
	{
		RebuildFromDefinition(*ctx.rebuild_def);
		std::cout << "[EditorRenderPipeline] PreRender rebuild: " << ctx.rebuild_def->graph_name << std::endl;
		delete ctx.rebuild_def;
		ctx.rebuild_def = nullptr;
		ctx.has_rebuild = false;
	}

	// Update backbuffer retained resources (safe on Render thread)
	RHI::Texture* rtv = m_viewport->GetCurrentBackBufferRTV();
	RHI::Texture* dsv = m_viewport->GetCurrentBackBufferDSV();

	if (auto* bb = graph.GetRetainedResource<RHI::TextureDesc, RHI::Texture>("BackBuffer"))
		bb->UpdateRetainedPtr(rtv);
	if (auto* ds = graph.GetRetainedResource<RHI::TextureDesc, RHI::Texture>("DepthStencil"))
		ds->UpdateRetainedPtr(dsv);
}

//  Render thread: record ImGui GPU commands
void EditorRenderPipeline::OnPostRender(Render::FrameContext& ctx)
{
	if (!ctx.imgui_context) return;

	// Save previous context — ImGui GImGui is global (not thread_local).
	// Never set to null; it would break GLFW callbacks on the Logic thread.
	auto* prev_ctx = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx.imgui_context));

	if (ctx.draw_data)
	{
		auto* cmd = RHIGetWriteCommandList();
		editor_ui.DrawFrame_Render(static_cast<ImDrawData*>(ctx.draw_data), cmd);
		ctx.draw_data = nullptr;
	}

	ImGui::SetCurrentContext(prev_ctx);
}

EditorRenderPipeline::EditorRenderPipeline(MXRender::PlatformWindow* in_window)
{
	// window will be set in OnInit
}

MXRender::PlatformWindow* EditorRenderPipeline::GetWindow()
{
	return m_window;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

