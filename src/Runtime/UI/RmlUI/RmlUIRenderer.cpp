#include "RmlUIRenderer.h"

#include "RHI/RenderRHI.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderShader.h"
#include "RHI/RenderTexture.h"
#include "RHI/RenderBuffer.h"
#include "RHI/RenderRource.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/Vulkan/VK_CommandBuffer.h"
#include "Tool/BufferUtils.h"

#include <fstream>
#include <iostream>
#include <cstring>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(RmlUI)

// =========================================================================
// Helper: Read SPIR-V binary from file
// =========================================================================
static Vector<UInt32> ReadShaderFile(CONST String& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	if (!file.is_open())
	{
		std::cerr << "[RmlUIRenderer] Failed to open shader: " << filename << std::endl;
		return {};
	}
	size_t fileSize = static_cast<size_t>(file.tellg());
	Vector<UInt32> buffer(fileSize / sizeof(UInt32));
	file.seekg(0);
	file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
	file.close();
	return buffer;
}

// =========================================================================
// Lifecycle
// =========================================================================
RmlUIRenderer::~RmlUIRenderer()
{
	Shutdown();
}

void RmlUIRenderer::Initialize(RHI::CommandList* cmd_list, RHI::Texture* backbuffer_rtv, RHI::Texture* backbuffer_dsv, UInt32 viewport_w, UInt32 viewport_h)
{
	m_backbuffer_rtv = backbuffer_rtv;
	m_backbuffer_dsv = backbuffer_dsv;
	m_viewport_w = viewport_w;
	m_viewport_h = viewport_h;

	for (int i = 0; i < 16; i++)
		m_transform[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	m_translation[0] = 0.0f;
	m_translation[1] = 0.0f;

	// Per-draw uniform buffer (Storage|Dynamic). Pre-allocate a sub-allocation
	// so that the buffer descriptor (SetResource below) gets the correct offset.
	m_per_draw_buf = Tool::BufferUtils::CreateDynamicParamBuffer(sizeof(PerDrawData));
	void* prealloc = m_per_draw_buf->Map(ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::Discard);
	m_per_draw_buf->Unmap();

	CreateShaders(cmd_list);
	CreatePSOs();

	std::cout << "[RmlUIRenderer] Initialized with " << m_geometries.size()
		<< " geometries, " << m_textures.size() << " textures." << std::endl;
}

void RmlUIRenderer::Shutdown()
{
	// [AI] Properly destroy geometry buffers before clearing the map.
	// ~VK_Buffer() calls Destroy()->FreeAllocation() which returns the sub-allocation
	// to the pooled page. Without this, leaked sub-allocations cause
	// DestroyResourceAllocations CHECK_WITH_LOG at shutdown.
	for (auto& kv : m_geometries) {
		delete kv.second.vb;
		delete kv.second.ib;
	}
	m_geometries.clear();
	for (auto& kv : m_textures) { delete kv.second.srb; delete kv.second.texture; }
	m_textures.clear();

	delete m_srb_untextured; m_srb_untextured = nullptr;
	delete m_per_draw_buf; m_per_draw_buf = nullptr;

	DestroyPSOs();
	DestroyShaders();
}

// =========================================================================
// BeginFrame / EndFrame
// =========================================================================
void RmlUIRenderer::BeginFrame(RHI::CommandList* cmd)
{
	m_current_cmd = cmd;
	m_stats = {};
	m_scissor_enabled = false;
	m_clip_mask_enabled = false;

	// Refresh viewport dimensions from viewport (handles window resize).
	// Use Viewport instead of cached RTV pointer — swapchain recreation
	// invalidates the old RTV on resize.
	if (m_viewport)
	{
		auto size = m_viewport->GetViewportSize();
		m_viewport_w = size[0];
		m_viewport_h = size[1];
	}
	// Transition any textures created outside render loop (font loading)
	for (auto* t : m_pending_transitions)
		m_current_cmd->TransitionTextureState(t, ENUM_RESOURCE_STATE::ShaderResource);
	m_pending_transitions.clear();
	// Dynamic buffers use a sub-allocator whose state tracking doesn't align
	// with fence signaling. v1: accumulate (RmlUI reuses geometry handles,
	// so the map naturally caps leaked count at ~text elements per frame).
}

void RmlUIRenderer::EndFrame(RHI::CommandList* cmd)
{
	m_current_cmd = nullptr;
}

// =========================================================================
// Geometry
// =========================================================================
UIGeometryHandle RmlUIRenderer::CompileGeometry(
	CONST void* vertices, UInt32 vtx_count, UInt32 vtx_stride,
	CONST void* indices, UInt32 idx_count, bool idx32)
{
	if (!g_render_rhi) return {};

	UIGeometryHandle h; h.value = m_next_geo_handle++; 
	GeoSlot slot;

	{
		RHI::BufferDesc desc;
		desc.size = vtx_count * vtx_stride;
		desc.stride = vtx_stride;
		desc.type = ENUM_BUFFER_TYPE::Vertex | ENUM_BUFFER_TYPE::Dynamic;
		slot.vb = g_render_rhi->CreateBuffer(desc);
		if (slot.vb)
		{
			void* mapped = slot.vb->Map(ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::Discard);
			if (mapped)
			{
				memcpy(mapped, vertices, desc.size);
				slot.vb->Unmap();
			}
		}
		slot.vtx_count = vtx_count;
		slot.vtx_stride = vtx_stride;
	}

	if (indices && idx_count > 0)
	{
		UInt32 idx_size = idx32 ? 4 : 2;
		RHI::BufferDesc desc;
		desc.size = idx_count * idx_size;
		desc.stride = idx_size;
		desc.type = ENUM_BUFFER_TYPE::Index | ENUM_BUFFER_TYPE::Dynamic;
		slot.ib = g_render_rhi->CreateBuffer(desc);
		if (slot.ib)
		{
			void* mapped = slot.ib->Map(ENUM_MAP_TYPE::Write, ENUM_MAP_FLAG::Discard);
			if (mapped)
			{
				memcpy(mapped, indices, desc.size);
				slot.ib->Unmap();
			}
		}
		slot.idx_count = idx_count;
	}

	m_geometries[h.value] = slot;
	m_stats.geometry_compiled++;
	return h;
}

void RmlUIRenderer::DrawGeometry(UIGeometryHandle geo, CONST void* transform, UITextureHandle tex)
{
	if (!m_current_cmd) return;

	auto it = m_geometries.find(geo.value);
	if (it == m_geometries.end()) return;

	CONST GeoSlot& g = it->second;
	if (!g.vb) return;

	if (transform)
	{
		memcpy(m_transform, transform, sizeof(float) * 16);
	}
	// m_translation is set by caller (RmlUIRenderInterface passes per-geometry translation)

	bool has_texture = (tex.value != 0);
	RHI::RenderPipelineState* pso = SelectPSO(has_texture);
	if (!pso) return;

	m_current_cmd->SetGraphicsPipeline(pso);

	if (has_texture)
	{
		auto tex_it = m_textures.find(tex.value);
		if (tex_it != m_textures.end() && tex_it->second.srb)
		{
			m_current_cmd->SetShaderResourceBinding(tex_it->second.srb);
		}
	}
	else
	{
		if (m_srb_untextured)
		{
			m_current_cmd->SetShaderResourceBinding(m_srb_untextured);
		}
	}

	UploadPerDrawData();
	m_current_cmd->SetVertexBuffer(g.vb, 0, g.vtx_stride, 0);

	if (g.ib && g.idx_count > 0)
	{
		m_current_cmd->SetIndexBuffer(g.ib, 0, true);
		m_current_cmd->DrawIndexed(g.idx_count, 1, 0, 0, 0);
	}
	else
	{
		DrawAttribute da;
		da.vertexCount = g.vtx_count;
		da.instanceCount = 1;
		m_current_cmd->Draw(da);
	}

	m_stats.draw_calls++;
}

void RmlUIRenderer::ReleaseGeometry(UIGeometryHandle geo)
{
	auto it = m_geometries.find(geo.value);
	if (it == m_geometries.end()) return;
	// [AI] Properly free the GPU buffers before erasing the map entry.
	// ~VK_Buffer() calls Destroy()->FreeAllocation() which returns the sub-allocation
	// to the pooled page. With frame_freed now properly set (Issue 3b),
	// the page gets a 3-frame grace period before OS release.
	delete it->second.vb;
	delete it->second.ib;
	m_geometries.erase(it);
}

// =========================================================================
// Texture
// =========================================================================
UITextureHandle RmlUIRenderer::CreateTexture(CONST void* pixel_data, UInt32 w, UInt32 h)
{
	if (!g_render_rhi || !pixel_data) return {};

	UITextureHandle h_t; h_t.value = m_next_tex_handle++; 
	TexSlot slot;

	RHI::TextureDesc desc;
	desc.width = w;
	desc.height = h;
	desc.mip_level = 1;
	desc.layer_count = 1;
	desc.format = ENUM_TEXTURE_FORMAT::RGBA8;
	desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
	desc.resource_state = ENUM_RESOURCE_STATE::ShaderResource;

	slot.texture = g_render_rhi->CreateTexture(desc);
	if (slot.texture)
	{
		RHI::TextureDataPayload payload;
		payload.data.resize(w * h * 4);
		memcpy(payload.data.data(), pixel_data, w * h * 4);
		payload.format = ENUM_TEXTURE_FORMAT::RGBA8;
		payload.width = w;
		payload.height = h;
		slot.texture->UpdateTextureData(payload);
		// Defer layout transition: font textures are created outside render loop
		m_pending_transitions.push_back(slot.texture);

		if (m_pso_textured)
		{
			m_pso_textured->CreateShaderResourceBinding(slot.srb, false);
			if (slot.srb)
			{
				slot.srb->SetResource("tex", slot.texture);
				slot.srb->SetResource("pc", m_per_draw_buf);
				slot.srb->FlushDescriptorWrites();
			}
		}
	}

	m_textures[h_t.value] = slot;
	m_stats.textures_created++;
	return h_t;
}

void RmlUIRenderer::ReleaseTexture(UITextureHandle tex)
{
	auto it = m_textures.find(tex.value);
	if (it == m_textures.end()) return;

	delete it->second.srb;
	delete it->second.texture;
	m_textures.erase(it);
}

// =========================================================================
// Scissor
// =========================================================================
void RmlUIRenderer::EnableScissor(bool enable)
{
	m_scissor_enabled = enable;
	if (m_current_cmd)
		static_cast<RHI::Vulkan::VK_CommandBuffer*>(m_current_cmd)->SetScissorEnable(enable);
}

void RmlUIRenderer::SetScissor(Int x, Int y, UInt32 w, UInt32 h)
{
	m_scissor_x = x;
	m_scissor_y = y;
	m_scissor_w = w;
	m_scissor_h = h;

	if (m_current_cmd)
	{
		// [AI] Y passed straight through: RmlUi scissor uses top-left origin (y=0=top),
		// Vulkan VkRect2D offset.y is also top-left with positive-height viewport.
		static_cast<RHI::Vulkan::VK_CommandBuffer*>(m_current_cmd)->SetScissor(x, y, w, h);
	}
}

// =========================================================================
// Clip mask (stencil)
// =========================================================================
void RmlUIRenderer::EnableClipMask(bool enable)
{
	m_clip_mask_enabled = enable;
	m_stats.used_clip_mask = m_stats.used_clip_mask || enable;
}

void RmlUIRenderer::RenderToClipMask(Int operation, UIGeometryHandle geo, CONST void* transform)
{
	if (!m_current_cmd) return;

	auto it = m_geometries.find(geo.value);
	if (it == m_geometries.end()) return;
	CONST GeoSlot& g = it->second;
	if (!g.vb) return;

	RHI::RenderPipelineState* pso = (operation == 2)
		? m_pso_stencil_intersect
		: m_pso_stencil_set;
	if (!pso) return;
	m_current_cmd->SetGraphicsPipeline(pso);

	UploadPerDrawData();
	m_current_cmd->SetVertexBuffer(g.vb, 0, g.vtx_stride, 0);

	if (g.ib && g.idx_count > 0)
	{
		m_current_cmd->SetIndexBuffer(g.ib, 0, true);
		m_current_cmd->DrawIndexed(g.idx_count, 1, 0, 0, 0);
	}
	else
	{
		DrawAttribute da;
		da.vertexCount = g.vtx_count;
		da.instanceCount = 1;
		m_current_cmd->Draw(da);
	}
}

// =========================================================================
// Transform
// =========================================================================
void RmlUIRenderer::SetTransform(CONST void* transform)
{
	if (transform)
	{
		memcpy(m_transform, transform, sizeof(float) * 16);
		m_stats.used_transform = true;
	}
	else
	{
		for (int i = 0; i < 16; i++)
			m_transform[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	}
}

void RmlUIRenderer::SetTranslation(Float32 x, Float32 y)
{
	m_translation[0] = x;
	m_translation[1] = y;
}

bool RmlUIRenderer::NeedsOffscreen() CONST
{
	return m_clip_mask_enabled || m_stats.used_layers || m_stats.used_transform;
}

UInt32 RmlUIRenderer::GetViewportHeight() CONST
{
	return m_viewport_h;
}

// =========================================================================
// PSO selection
// =========================================================================
RHI::RenderPipelineState* RmlUIRenderer::SelectPSO(bool has_texture) CONST
{
	if (m_clip_mask_enabled)
	{
		return has_texture ? m_pso_textured_stencil : m_pso_untextured_stencil;
	}
	return has_texture ? m_pso_textured : m_pso_untextured;
}

// =========================================================================
// Per-draw uniform buffer upload (SSBO, MeshSample pattern)
// =========================================================================
void RmlUIRenderer::UploadPerDrawData()
{
	if (!m_per_draw_buf) return;

	// Orthographic projection: pixel coords [0,w]x[0,h] -> Vulkan NDC [-1,1]^2 (Y-flip)
	float w = (float)(m_viewport_w > 0 ? m_viewport_w : 1);
	float h = (float)(m_viewport_h > 0 ? m_viewport_h : 1);
	float proj[16] = {
		2.0f/w,  0,      0, 0,
		0,      2.0f/h,  0, 0,
		0,       0,      0, 0,
		-1.0f, -1.0f,    0, 1
	};

	// final = proj x m_transform  (both column-major)
	float a[16]; memcpy(a, proj, sizeof(a));
	float b[16]; memcpy(b, m_transform, sizeof(b));
	float final[16] = {};
	for (int col = 0; col < 4; ++col) {
		for (int row = 0; row < 4; ++row) {
			final[row + col * 4] =
				a[row + 0 * 4] * b[0 + col * 4] +
				a[row + 1 * 4] * b[1 + col * 4] +
				a[row + 2 * 4] * b[2 + col * 4] +
				a[row + 3 * 4] * b[3 + col * 4];
		}
	}

	PerDrawData data;
	memcpy(data.transform, final, sizeof(data.transform));
	data.translation[0] = m_translation[0];
	data.translation[1] = m_translation[1];

	Tool::BufferUtils::Upload(m_per_draw_buf, &data, sizeof(data));
}

// =========================================================================
// Shader creation
// =========================================================================
void RmlUIRenderer::CreateShaders(RHI::CommandList* cmd_list)
{
	RHI::ShaderDesc sd;
	RHI::ShaderDataPayload sp;

	sd.shader_type = ENUM_SHADER_STAGE::Shader_Vertex;
	sd.shader_name = "RmlUIVS";
	sd.entry_name = "main";
	sp.data = ReadShaderFile("Shader/rml_ui.vert.spv");
	if (!sp.data.empty())
		m_vs = g_render_rhi->CreateShader(sd, sp);

	sd.shader_type = ENUM_SHADER_STAGE::Shader_Pixel;
	sd.shader_name = "RmlUIFS";
	sp.data = ReadShaderFile("Shader/rml_ui.frag.spv");
	if (!sp.data.empty())
		m_fs_textured = g_render_rhi->CreateShader(sd, sp);

	sd.shader_name = "RmlUICompositeFS";
	sp.data = ReadShaderFile("Shader/rml_ui_composite.frag.spv");
	if (!sp.data.empty())
		m_fs_composite = g_render_rhi->CreateShader(sd, sp);

	std::cout << "[RmlUIRenderer] Shaders loaded: vs=" << (m_vs != nullptr)
		<< " fs_tex=" << (m_fs_textured != nullptr)
		<< " fs_comp=" << (m_fs_composite != nullptr) << std::endl;
}

void RmlUIRenderer::DestroyShaders()
{
	delete m_vs; m_vs = nullptr;
	delete m_fs_textured; m_fs_textured = nullptr;
	delete m_fs_composite; m_fs_composite = nullptr;
}

// =========================================================================
// PSO creation
// =========================================================================
void RmlUIRenderer::CreatePSOs()
{
	if (!g_render_rhi || !m_vs || !m_fs_textured) return;

	auto MakeBaseDesc = [&]() {
		RHI::RenderGraphiPipelineStateDesc desc;
		desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = m_vs;
		desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = m_fs_textured;
		desc.render_targets = { m_backbuffer_rtv };
		desc.depth_stencil_view = m_backbuffer_dsv;
		desc.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
		desc.raster_state.cull_mode = ENUM_RASTER_CULLMODE::None;
		desc.raster_state.fill_mode = ENUM_RASTER_FILLMODE::Solid;
		desc.raster_state.front_counter_clockwise = true;
		desc.raster_state.scissor_enable = true;
		desc.raster_state.sample_count = 1;

		desc.blend_state.render_targets.resize(1);
		desc.blend_state.render_targets[0].blend_enable = true;
		desc.blend_state.render_targets[0].src_color = ENUM_BLEND_FACTOR::ENUM_SRC_ALPHA;
		desc.blend_state.render_targets[0].dst_color = ENUM_BLEND_FACTOR::ENUM_ONE_MINUS_SRC_ALPHA;
		desc.blend_state.render_targets[0].src_alpha = ENUM_BLEND_FACTOR::ENUM_ONE;
		desc.blend_state.render_targets[0].dst_alpha = ENUM_BLEND_FACTOR::ENUM_ONE_MINUS_SRC_ALPHA;
		desc.blend_state.render_targets[0].op_color = ENUM_BLEND_EQUATION::ENUM_ADD;
		desc.blend_state.render_targets[0].op_alpha = ENUM_BLEND_EQUATION::ENUM_ADD;
		desc.blend_state.render_targets[0].write_mask = ENUM_COLOR_MASK::All;

		desc.depth_stencil_state.depth_test_enable = false;
		desc.depth_stencil_state.depth_write_enable = false;
		desc.depth_stencil_state.stencil_test_enable = false;

		RHI::VertexInputLayout pos_layout;
		pos_layout.binding = 0;
		pos_layout.location = 0;
		pos_layout.attribute_format = ENUM_TEXTURE_FORMAT::RG32F;
		pos_layout.offset = 0;
		desc.vertex_input_layout.push_back(pos_layout);

		RHI::VertexInputLayout color_layout;
		color_layout.binding = 0;
		color_layout.location = 1;
		color_layout.attribute_format = ENUM_TEXTURE_FORMAT::RGBA8;
		color_layout.offset = 8;
		desc.vertex_input_layout.push_back(color_layout);

		RHI::VertexInputLayout uv_layout;
		uv_layout.binding = 0;
		uv_layout.location = 2;
		uv_layout.attribute_format = ENUM_TEXTURE_FORMAT::RG32F;
		uv_layout.offset = 12;
		desc.vertex_input_layout.push_back(uv_layout);

		return desc;
	};

	// PSO 1: Textured
	{
		auto desc = MakeBaseDesc();
		m_pso_textured = g_render_rhi->CreateRenderPipelineState(desc);
		if (m_pso_textured)
		{
			m_pso_textured->CreateShaderResourceBinding(m_srb_untextured, false);
			if (m_srb_untextured)
			{
				m_srb_untextured->SetResource("pc", m_per_draw_buf);
				m_srb_untextured->FlushDescriptorWrites();
			}
			// Bind a 1x1 white dummy texture so the descriptor is never uninitialized
			RHI::TextureDesc dummy_desc;
			dummy_desc.width = 1; dummy_desc.height = 1; dummy_desc.mip_level = 1; dummy_desc.layer_count = 1;
			dummy_desc.format = ENUM_TEXTURE_FORMAT::RGBA8;
			dummy_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
			dummy_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
			dummy_desc.resource_state = ENUM_RESOURCE_STATE::ShaderResource;
			auto* dummy_tex = g_render_rhi->CreateTexture(dummy_desc);
			if (dummy_tex)
			{
				UInt8 white[4] = {255, 255, 255, 255};
				RHI::TextureDataPayload payload;
				payload.data.assign(white, white + 4);
				payload.format = ENUM_TEXTURE_FORMAT::RGBA8;
				payload.width = 1; payload.height = 1;
				dummy_tex->UpdateTextureData(payload);
				m_srb_untextured->SetResource("tex", dummy_tex);
				m_srb_untextured->FlushDescriptorWrites();
			}
		}
	}
	m_pso_untextured = m_pso_textured;

	// PSO 2: Stencil Set
	{
		auto desc = MakeBaseDesc();
		desc.blend_state.render_targets[0].write_mask = static_cast<ENUM_COLOR_MASK>(0);
		desc.depth_stencil_state.stencil_test_enable = true;
		desc.depth_stencil_state.stencil_write_mask = 0xFF;
		desc.depth_stencil_state.stencil_ref = 1;
		desc.depth_stencil_state.front_face_stencil.func = ENUM_STENCIL_FUNCTION::ENUM_ALWAYS;
		desc.depth_stencil_state.front_face_stencil.pass_op = ENUM_STENCIL_OPERATIOON::ENUM_REPLACE;
		desc.depth_stencil_state.front_face_stencil.fail_op = ENUM_STENCIL_OPERATIOON::ENUM_KEEP;
		desc.depth_stencil_state.front_face_stencil.depth_fail_op = ENUM_STENCIL_OPERATIOON::ENUM_KEEP;
		desc.depth_stencil_state.back_face_stencil = desc.depth_stencil_state.front_face_stencil;
		m_pso_stencil_set = g_render_rhi->CreateRenderPipelineState(desc);
	}

	// PSO 3: Stencil Intersect
	{
		auto desc = MakeBaseDesc();
		desc.blend_state.render_targets[0].write_mask = static_cast<ENUM_COLOR_MASK>(0);
		desc.depth_stencil_state.stencil_test_enable = true;
		desc.depth_stencil_state.stencil_read_mask = 0xFF;
		desc.depth_stencil_state.stencil_write_mask = 0xFF;
		desc.depth_stencil_state.front_face_stencil.func = ENUM_STENCIL_FUNCTION::ENUM_EQUAL;
		desc.depth_stencil_state.front_face_stencil.pass_op = ENUM_STENCIL_OPERATIOON::ENUM_REPLACE;
		desc.depth_stencil_state.front_face_stencil.fail_op = ENUM_STENCIL_OPERATIOON::ENUM_KEEP;
		desc.depth_stencil_state.front_face_stencil.depth_fail_op = ENUM_STENCIL_OPERATIOON::ENUM_KEEP;
		desc.depth_stencil_state.back_face_stencil = desc.depth_stencil_state.front_face_stencil;
		m_pso_stencil_intersect = g_render_rhi->CreateRenderPipelineState(desc);
	}

	// PSO 4: Textured + Stencil Test
	{
		auto desc = MakeBaseDesc();
		desc.depth_stencil_state.stencil_test_enable = true;
		desc.depth_stencil_state.stencil_read_mask = 0xFF;
		desc.depth_stencil_state.stencil_write_mask = 0;
		desc.depth_stencil_state.front_face_stencil.func = ENUM_STENCIL_FUNCTION::ENUM_EQUAL;
		desc.depth_stencil_state.front_face_stencil.pass_op = ENUM_STENCIL_OPERATIOON::ENUM_KEEP;
		desc.depth_stencil_state.front_face_stencil.fail_op = ENUM_STENCIL_OPERATIOON::ENUM_KEEP;
		desc.depth_stencil_state.front_face_stencil.depth_fail_op = ENUM_STENCIL_OPERATIOON::ENUM_KEEP;
		desc.depth_stencil_state.back_face_stencil = desc.depth_stencil_state.front_face_stencil;
		m_pso_textured_stencil = g_render_rhi->CreateRenderPipelineState(desc);
	}
	m_pso_untextured_stencil = m_pso_textured_stencil;

	// PSO 5: Composite
	{
		auto desc = MakeBaseDesc();
		desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = m_fs_composite;
		m_pso_composite = g_render_rhi->CreateRenderPipelineState(desc);
	}

	std::cout << "[RmlUIRenderer] PSOs created" << std::endl;
}

void RmlUIRenderer::DestroyPSOs()
{
	m_pso_textured = nullptr;
	m_pso_untextured = nullptr;
	m_pso_stencil_set = nullptr;
	m_pso_stencil_intersect = nullptr;
	m_pso_textured_stencil = nullptr;
	m_pso_untextured_stencil = nullptr;
	m_pso_composite = nullptr;
}

// =========================================================================
// Composite layer (Mode B)
// =========================================================================
void RmlUIRenderer::CompositeLayer(RHI::Texture* ui_layer_texture, UInt32 tex_width, UInt32 tex_height)
{
	if (!m_current_cmd || !m_pso_composite || !ui_layer_texture) return;

	m_current_cmd->SetGraphicsPipeline(m_pso_composite);
	(void)tex_width; (void)tex_height;

	DrawAttribute da;
	da.vertexCount = 3;
	da.instanceCount = 1;
	m_current_cmd->Draw(da);
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
