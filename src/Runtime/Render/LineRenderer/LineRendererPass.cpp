#include "Render/LineRenderer/LineRendererPass.h"
#include "Render/LineRenderer/LineRendererManager.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/RenderBuffer.h"
#include "RHI/RenderTexture.h"
#include "Tool/ShaderLibrary.h"
#include "Tool/BufferUtils.h"
#include <glm/glm.hpp>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)

using namespace MXRender;
using namespace MXRender::RHI;

namespace
{
	// Vertex: position RG32F + color RGBA32F (24B), aligned with line_trail.vert.
	struct LineVertex
	{
		glm::vec2 pos{ 0.0f, 0.0f };
		glm::vec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	};

	// Segment -> 6-vertex quad (2 triangles). Width/color lerped at endpoints.
	// Zero-area segments (p0==p1) are naturally invisible.
	void ExpandSegment(const glm::vec2& a, const glm::vec2& b,
		Float32 hw_a, Float32 hw_b,
		const glm::vec4& ca, const glm::vec4& cb, LineVertex* out)
	{
		glm::vec2 d = b - a;
		if (glm::dot(d, d) < 1e-8f)
			d = glm::vec2(1.0f, 0.0f);
		else
			d = glm::normalize(d);
		const glm::vec2 perp(-d.y, d.x);
		const glm::vec2 a0 = a + perp * hw_a;
		const glm::vec2 a1 = a - perp * hw_a;
		const glm::vec2 b0 = b + perp * hw_b;
		const glm::vec2 b1 = b - perp * hw_b;
		out[0] = { a0, ca };  out[1] = { b0, cb };  out[2] = { a1, ca };   // tri 1
		out[3] = { b0, cb };  out[4] = { b1, cb };  out[5] = { a1, ca };   // tri 2
	}

	struct LinePassData : public RenderGraphPassDataBase
	{
		RenderPipelineState* pso = nullptr;              // owned by PipelineStateManager, never delete
		ShaderResourceBinding* srb = nullptr;            // deleted in Release()
		RHI::Buffer* vertex_buffer = nullptr;            // shared dynamic VB, deleted in Release()
		RHI::Buffer* params_buffer = nullptr;            // shared MVP, deleted in Release()
		VIRTUAL ~LinePassData() { Release(); }
		VIRTUAL void Release() OVERRIDE
		{
			delete srb; srb = nullptr;
			delete vertex_buffer; vertex_buffer = nullptr;
			delete params_buffer; params_buffer = nullptr;
		}
	};

		// CPU assembly buffers (file-static: render-thread exclusive, capacity
		// retained = zero per-frame allocation; execute lambda param is const,
		// so they cannot live on the pass data)
	// execute lambda 参数是 const，不能放 pass 数据成员）
	Vector<LineVertex> s_vertex_blob;
	struct DrawItem { UInt32 first_vertex = 0; UInt32 vertex_count = 0; };
	Vector<DrawItem> s_draw_list;
}

void RegisterLinePass(
	RenderGraph* graph,
	RenderGraphResource<RHI::TextureDesc, RHI::Texture>* bb_resource,
	RHI::CommandList* cmd_list)
{
	auto* pass = graph->AddRenderPass<LinePassData>("LinePass", graph, cmd_list,
		[bb_resource](LinePassData& data, RenderGraphPassBuilder& builder, RHI::CommandList* cmd)
		{
			(void)cmd;
			builder.Write(bb_resource);   // overlays the backbuffer (load mode, no clear)

			Shader* vs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Vertex,
				"Shader/line_trail.vert.spv");
			Shader* ps = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Pixel,
				"Shader/line_trail.frag.spv");

			RenderGraphiPipelineStateDesc pd{};
			pd.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
			pd.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps;
			pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
			pd.render_targets = { bb_resource->GetActual() };
			pd.raster_state.sample_count = 1;
			pd.raster_state.cull_mode = ENUM_RASTER_CULLMODE::None;
			pd.blend_state.render_targets.resize(1);   // REQUIRED - missing = pipeline creation failure = black screen

			VertexInputLayout pos_layout;
			pos_layout.binding = 0;
			pos_layout.location = 0;
			pos_layout.attribute_format = ENUM_TEXTURE_FORMAT::RG32F;
			pos_layout.input_rate = ENUM_VERTEX_INPUTRATE::PerVertex;
			pos_layout.offset = 0;
			VertexInputLayout color_layout;
			color_layout.binding = 0;
			color_layout.location = 1;
			color_layout.attribute_format = ENUM_TEXTURE_FORMAT::RGBA32F;
			color_layout.input_rate = ENUM_VERTEX_INPUTRATE::PerVertex;
			color_layout.offset = (UInt32)(sizeof(float) * 2);
			pd.vertex_input_layout.push_back(pos_layout);
			pd.vertex_input_layout.push_back(color_layout);

			data.pso = g_render_rhi->CreateRenderPipelineState(pd);
			data.pso->CreateShaderResourceBinding(data.srb, false);

			// Shared dynamic VB: kMaxRenderers x (kMaxPoints-1) segments x 6 verts x 24B
			const UInt32 kMaxSegments = (LineRendererManager::kMaxRenderers) * (LineRendererComponent::kMaxPoints - 1);
			RHI::BufferDesc vb_desc;
			vb_desc.type = ENUM_BUFFER_TYPE::Vertex | ENUM_BUFFER_TYPE::Dynamic;
			vb_desc.size = kMaxSegments * 6 * sizeof(LineVertex);
			vb_desc.stride = sizeof(LineVertex);
			data.vertex_buffer = g_render_rhi->CreateBuffer(vb_desc);
			data.params_buffer = Tool::BufferUtils::CreateDynamicParamBuffer(sizeof(glm::mat4));
			s_vertex_blob.reserve(kMaxSegments * 6);

			data.srb->SetResource("params", data.params_buffer);   // GLSL instance name
			data.srb->FlushDescriptorWrites();

			delete vs;
			delete ps;
		},
		[bb_resource](CONST LinePassData& data, RHI::CommandList* in_cmd)
		{
			LineRendererManager& mgr = LineRendererManager::Get();
			const UInt32 n = mgr.GetRenderCount();
			if (n == 0)
				return;
			const LineFrameState* states = mgr.GetRenderStates();

			// MVP (independent params buffer, single Upload)
			glm::mat4 mvp = mgr.GetCameraMVP();
			Tool::BufferUtils::Upload(data.params_buffer, &mvp, sizeof(mvp));

			// CPU-expand all trail segments into the shared VB (single buffer, single Upload)
			s_vertex_blob.clear();
			s_draw_list.clear();
			for (UInt32 i = 0; i < n; ++i)
			{
				const LineFrameState& st = states[i];
				if (!st.visible || st.point_count < 2)
					continue;
				const UInt32 segs = st.loop ? st.point_count : st.point_count - 1;
				const UInt32 first = (UInt32)s_vertex_blob.size();
				for (UInt32 s = 0; s < segs; ++s)
				{
					const Float32 t0 = (Float32)s / (Float32)(segs > 0 ? segs : 1);
					const Float32 t1 = (Float32)(s + 1) / (Float32)(segs > 0 ? segs : 1);
					const glm::vec2 a = st.points[s];
					const glm::vec2 b = st.points[(s + 1) % st.point_count];
					const Float32 hw_a = glm::mix(st.start_width, st.end_width, t0) * 0.5f;
					const Float32 hw_b = glm::mix(st.start_width, st.end_width, t1) * 0.5f;
					const glm::vec4 ca = glm::mix(st.start_color, st.end_color, t0);
					const glm::vec4 cb = glm::mix(st.start_color, st.end_color, t1);
					LineVertex quad[6];
					ExpandSegment(a, b, hw_a, hw_b, ca, cb, quad);
					s_vertex_blob.insert(s_vertex_blob.end(), quad, quad + 6);
				}
				s_draw_list.push_back({ first, (UInt32)(s_vertex_blob.size() - first) });
			}
			if (s_vertex_blob.empty())
				return;
			Tool::BufferUtils::Upload(data.vertex_buffer, s_vertex_blob.data(),
				(UInt32)(s_vertex_blob.size() * sizeof(LineVertex)));

			// Load-mode render to the backbuffer (no clear, no dsv - same as PlayerPass)
			Vector<RHI::Texture*> rtvs = { bb_resource->GetActual() };
			in_cmd->SetRenderTarget(rtvs, nullptr, Vector<ClearValue>{}, false);
			in_cmd->SetGraphicsPipeline(data.pso);
			in_cmd->SetShaderResourceBinding(data.srb);
			in_cmd->SetVertexBuffer(data.vertex_buffer, 0, sizeof(LineVertex), 0);
			for (CONST auto& d : s_draw_list)
				in_cmd->Draw({ d.vertex_count, 1, d.first_vertex, 0 });
		});
	pass->SetIsCullable(false);
	pass->SetShaderPath("Shader/line_trail");
}

MYRENDERER_END_NAMESPACE  // Render
MYRENDERER_END_NAMESPACE  // MXRender
