#pragma once
#ifndef _RMLUIRENDERER_
#define _RMLUIRENDERER_

#include "Core/ConstDefine.h"
#include "RHI/RenderEnum.h"
#include "UI/UIRenderer.h"
#include "UI/UIDrawBuffer.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Viewport;
MYRENDERER_END_NAMESPACE
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Buffer;
class Texture;
class Shader;
class RenderPipelineState;
class ShaderResourceBinding;
class CommandList;
MYRENDERER_END_NAMESPACE
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(RmlUI)

/**
 * Concrete UIRenderer implementation for RmlUI on Vulkan.
 *
 * Owns the GPU resources needed to draw RmlUI geometry:
 * - 6 PSOs (textured, untextured, 2 stencil variants, 2 stencil-test variants)
 * - Vertex/index buffer ring allocator for compiled geometry
 * - Texture cache (SRB per texture)
 * - Push constant management
 * - Scissor and stencil state tracking
 *
 * Thread safety: All methods must be called from the render thread
 * (inside the RDG pass execute lambda).
 */
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RmlUIRenderer, public UIRenderer)

#pragma region METHOD
public:
	RmlUIRenderer() MYDEFAULT;
	VIRTUAL ~RmlUIRenderer();

	/// Initialize Vulkan resources (shaders, PSOs). Must be called after RHI is ready.
	void METHOD(Initialize)(RHI::CommandList* cmd_list, RHI::Texture* backbuffer_rtv, RHI::Texture* backbuffer_dsv, UInt32 viewport_w, UInt32 viewport_h);
	/// Set viewport reference for resize-safe size query in BeginFrame.
	void METHOD(SetViewport)(RHI::Viewport* vp) { m_viewport = vp; }

	/// Release all Vulkan resources.
	void METHOD(Shutdown)();

	// === UIRenderer interface ===

	VIRTUAL void METHOD(BeginFrame)(RHI::CommandList* cmd) OVERRIDE;
	VIRTUAL void METHOD(EndFrame)(RHI::CommandList* cmd) OVERRIDE;

	VIRTUAL UIGeometryHandle METHOD(CompileGeometry)(
		CONST void* vertices, UInt32 vtx_count, UInt32 vtx_stride,
		CONST void* indices, UInt32 idx_count, bool idx32) OVERRIDE;

	VIRTUAL void METHOD(DrawGeometry)(
		UIGeometryHandle geo, CONST void* transform, UITextureHandle tex) OVERRIDE;

	VIRTUAL void METHOD(ReleaseGeometry)(UIGeometryHandle geo) OVERRIDE;

	VIRTUAL UITextureHandle METHOD(CreateTexture)(
		CONST void* pixel_data, UInt32 w, UInt32 h) OVERRIDE;

	VIRTUAL void METHOD(ReleaseTexture)(UITextureHandle tex) OVERRIDE;

	VIRTUAL void METHOD(EnableScissor)(bool enable) OVERRIDE;
	VIRTUAL void METHOD(SetScissor)(Int x, Int y, UInt32 w, UInt32 h) OVERRIDE;

	// Optional overrides
	VIRTUAL void METHOD(EnableClipMask)(bool enable) OVERRIDE;
	VIRTUAL void METHOD(RenderToClipMask)(Int operation, UIGeometryHandle geo, CONST void* transform) OVERRIDE;
	VIRTUAL void METHOD(SetTransform)(CONST void* transform) OVERRIDE;
	void METHOD(SetTranslation)(Float32 x, Float32 y);
	VIRTUAL bool METHOD(NeedsOffscreen)() CONST OVERRIDE;
	VIRTUAL UInt32 METHOD(GetViewportHeight)() CONST OVERRIDE;

	/// Composite an offscreen UI layer onto the backbuffer with alpha blending.
	/// Used by the RDG composite pass (Mode B).
	/// @param ui_layer_texture  The offscreen UI color texture (RGBA8)
	/// @param tex_width, tex_height  Texture dimensions (for fullscreen UV)
	void METHOD(CompositeLayer)(RHI::Texture* ui_layer_texture, UInt32 tex_width, UInt32 tex_height);

protected:
private:
	// Shaders
	RHI::Shader* m_vs = nullptr;
	RHI::Shader* m_fs_textured = nullptr;
	RHI::Shader* m_fs_composite = nullptr;

	// Pipeline states (7 total)
	RHI::RenderPipelineState* m_pso_textured = nullptr;
	RHI::RenderPipelineState* m_pso_composite = nullptr;    // fullscreen composite (Mode B)
	RHI::RenderPipelineState* m_pso_untextured = nullptr;
	RHI::RenderPipelineState* m_pso_stencil_set = nullptr;
	RHI::RenderPipelineState* m_pso_stencil_intersect = nullptr;
	RHI::RenderPipelineState* m_pso_textured_stencil = nullptr;
	RHI::RenderPipelineState* m_pso_untextured_stencil = nullptr;

	// SRB for untextured draw
	RHI::ShaderResourceBinding* m_srb_untextured = nullptr;

	// Per-draw uniform buffer (SSBO, Storage|Dynamic), Map/Discard each draw
	// Discard orphans the old allocation so GPU can still read previous frame's data
	RHI::Buffer* m_per_draw_buf = nullptr;
	struct PerDrawData { float transform[16]; float translation[2]; };

	// Current command list (set by BeginFrame)
	RHI::CommandList* m_current_cmd = nullptr;

	// Viewport height for scissor Y-flip
	UInt32 m_viewport_w = 0;
	UInt32 m_viewport_h = 0;

	// Geometry handle → buffer mapping
	struct GeoSlot {
		RHI::Buffer* vb = nullptr;
		RHI::Buffer* ib = nullptr;
		UInt32 vtx_count = 0;
		UInt32 idx_count = 0;
		UInt32 vtx_stride = 0;
	};
	Map<GenericHandle, GeoSlot> m_geometries;
	UInt32 m_next_geo_handle = 1;

	RHI::Texture* m_backbuffer_rtv = nullptr;
	RHI::Texture* m_backbuffer_dsv = nullptr;
	RHI::Viewport* m_viewport = nullptr;

	// Texture handle → texture mapping
	struct TexSlot {
		RHI::Texture* texture = nullptr;
		RHI::ShaderResourceBinding* srb = nullptr;
	};
	Map<GenericHandle, TexSlot> m_textures;
	Vector<RHI::Texture*> m_pending_transitions; // textures needing ShaderResource transition
	UInt32 m_next_tex_handle = 1;

	// Cached per-draw data (written to m_per_draw_buf each draw call)
	float m_transform[16];
	float m_translation[2];

	// Scissor state
	bool m_scissor_enabled = false;
	Int m_scissor_x = 0, m_scissor_y = 0;
	UInt32 m_scissor_w = 0, m_scissor_h = 0;

	// Clip mask state
	bool m_clip_mask_enabled = false;
	Int m_stencil_ref = 1;

	// Layer stack (0 = backbuffer)
	Int m_current_layer = 0;

	// Stats
	UIDrawStats m_stats;

	// Internal helpers
	void METHOD(CreatePSOs)();
	void METHOD(CreateShaders)(RHI::CommandList* cmd_list);
	void METHOD(DestroyPSOs)();
	void METHOD(DestroyShaders)();
	void METHOD(UploadPerDrawData)();

	/// Selects the appropriate PSO based on current state
	RHI::RenderPipelineState* METHOD(SelectPSO)(bool has_texture) CONST;
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif // !_RMLUIRENDERER_
