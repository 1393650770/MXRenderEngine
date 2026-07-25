#if PLATFORM_GLES3

#include "RHI/GLES3/GLES3_PipelineState.h"
#include "RHI/GLES3/GLES3_Utils.h"
#include "RHI/GLES3/GLES3_ShaderResourceBinding.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

GLES3_PipelineState::GLES3_PipelineState(CONST RenderGraphiPipelineStateDesc& in_desc)
	: RenderPipelineState(in_desc)
{
}

GLES3_PipelineState::~GLES3_PipelineState()
{
	if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
	if (m_program) { glDeleteProgram(m_program); m_program = 0; }
}

void GLES3_PipelineState::CreateShaderResourceBinding(ShaderResourceBinding*& out_srb, Bool init_static_resource)
{
	// HelloTriangle: empty SRB (no resources bound).
	out_srb = new GLES3_ShaderResourceBinding();
}

void GLES3_PipelineState::ApplyState(GLboolean depth_write) CONST
{
	CONST auto& ds = desc.depth_stencil_state;
	CONST auto& rs = desc.raster_state;
	CONST auto& bs = desc.blend_state;

	// Depth/Stencil
	if (ds.depth_test_enable)
	{
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(TranslateDepthFunction(ds.depth_func));
		glDepthMask(ds.depth_write_enable ? GL_TRUE : GL_FALSE);
	}
	else
	{
		glDisable(GL_DEPTH_TEST);
	}

	// Stencil
	if (ds.stencil_test_enable)
	{
		glEnable(GL_STENCIL_TEST);
		// Phase 2: full stencil state (op/func/mask per face)
	}
	else
	{
		glDisable(GL_STENCIL_TEST);
	}

	// Rasterizer
	ENUM_RASTER_CULLMODE cull = rs.cull_mode;
	if (cull == ENUM_RASTER_CULLMODE::None)
	{
		glDisable(GL_CULL_FACE);
	}
	else
	{
		glEnable(GL_CULL_FACE);
		glCullFace(TranslateCullMode(cull));
	}
	glPolygonOffset(rs.depth_bias_slope_scaled, rs.depth_bias);

	// Blend
	if (!bs.render_targets.empty() && bs.render_targets[0].blend_enable)
	{
		glEnable(GL_BLEND);
		glBlendFunc(TranslateBlendFactor(bs.render_targets[0].src_color),
		            TranslateBlendFactor(bs.render_targets[0].dst_color));
		glBlendEquation(TranslateBlendEquation(bs.render_targets[0].op_color));
	}
	else
	{
		glDisable(GL_BLEND);
	}
}

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
