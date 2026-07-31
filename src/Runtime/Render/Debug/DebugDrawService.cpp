#include "Render/Debug/DebugDrawService.h"
#include "Tool/BufferUtils.h"
#include "Tool/ShaderLibrary.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/RenderBuffer.h"
#include "RHI/RenderShader.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)

DebugDrawService* DebugDrawService::s_instance = nullptr;

DebugDrawService& DebugDrawService::Get()
{
	return *s_instance;
}

void DebugDrawService::Create()
{
	if (!s_instance)
		s_instance = new DebugDrawService();
}

void DebugDrawService::Destroy()
{
	delete s_instance;
	s_instance = nullptr;
}

void DebugDrawService::BeginFrame()
{
	line_buffer_.clear();
}

void DebugDrawService::DrawLine(glm::vec2 a, glm::vec2 b, glm::vec3 color)
{
	line_buffer_.push_back({ a, color });
	line_buffer_.push_back({ b, color });
}

void DebugDrawService::DrawRect(glm::vec2 min, glm::vec2 max, glm::vec3 color)
{
	DrawLine({ min.x, min.y }, { max.x, min.y }, color);
	DrawLine({ max.x, min.y }, { max.x, max.y }, color);
	DrawLine({ max.x, max.y }, { min.x, max.y }, color);
	DrawLine({ min.x, max.y }, { min.x, min.y }, color);
}

void DebugDrawService::DrawCircle(glm::vec2 center, Float32 radius, glm::vec3 color, UInt32 segments)
{
	glm::vec2 prev{ center.x + radius, center.y };
	for (UInt32 i = 1; i <= segments; ++i)
	{
		Float32 angle = 6.2831853f * (Float32)i / (Float32)segments;
		glm::vec2 next{ center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius };
		DrawLine(prev, next, color);
		prev = next;
	}
}

void DebugDrawService::DrawAABB(glm::vec2 min, glm::vec2 max, glm::vec3 color)
{
	DrawRect(min, max, color);
}

void DebugDrawService::EndFrame()
{
	// Nothing to finalize CPU-side; upload happens in Render().
}

void DebugDrawService::Render(RHI::CommandList* cmd, CONST glm::mat4& view_proj)
{
	if (line_buffer_.empty())
		return;

	if (!initialized_)
	{
		// Vertex|Dynamic for the line strip; mvp via Storage|Dynamic.
		RHI::BufferDesc vb_desc;
		vb_desc.type = ENUM_BUFFER_TYPE::Vertex | ENUM_BUFFER_TYPE::Dynamic;
		vb_desc.size = (UInt32)(sizeof(LineVertex) * 65536);
		vb_desc.stride = sizeof(LineVertex);
		vertex_buffer_ = g_render_rhi->CreateBuffer(vb_desc);
		mvp_buffer_ = Tool::BufferUtils::CreateDynamicParamBuffer(sizeof(glm::mat4));

		RHI::Shader* vs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Vertex,
			"Shader/debug_line.vert.spv");
		RHI::Shader* ps = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Pixel,
			"Shader/debug_line.frag.spv");

		RHI::RenderGraphiPipelineStateDesc pd{};
		pd.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
		pd.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps;
		pd.primitive_topology = ENUM_PRIMITIVE_TYPE::LineList;
		pd.raster_state.sample_count = 1;
		pd.raster_state.cull_mode = ENUM_RASTER_CULLMODE::None;
		pso_ = g_render_rhi->CreateRenderPipelineState(pd);
		delete vs;
		delete ps;

		pso_->CreateShaderResourceBinding(srb_, false);
		srb_->SetResource("mvp", mvp_buffer_);
		srb_->FlushDescriptorWrites();
		initialized_ = true;
	}

	UInt32 bytes = (UInt32)(line_buffer_.size() * sizeof(LineVertex));
	Tool::BufferUtils::Upload(vertex_buffer_, line_buffer_.data(), bytes);
	Tool::BufferUtils::Upload(mvp_buffer_, &view_proj, sizeof(glm::mat4));

	cmd->SetGraphicsPipeline(pso_);
	cmd->SetShaderResourceBinding(srb_);
	cmd->SetVertexBuffer(vertex_buffer_, 0, sizeof(LineVertex), 0);
	DrawAttribute draw{};
	draw.vertexCount = (UInt32)line_buffer_.size();
	draw.instanceCount = 1;
	cmd->Draw(draw);
}

DebugDrawService::~DebugDrawService()
{
	delete srb_;
	srb_ = nullptr;
	delete vertex_buffer_;
	vertex_buffer_ = nullptr;
	delete mvp_buffer_;
	mvp_buffer_ = nullptr;
}

MYRENDERER_END_NAMESPACE  // Render
MYRENDERER_END_NAMESPACE  // MXRender