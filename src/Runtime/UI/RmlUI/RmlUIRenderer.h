#pragma once
#ifndef _RMLUIRENDERER_
#define _RMLUIRENDERER_

#include "Core/ConstDefine.h"
#include "RHI/RenderEnum.h"
#include "UI/UIRenderer.h"
#include "UI/UIHandleTypes.h"
#include "UI/UIDrawBuffer.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Viewport;
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
 * Concrete UIRenderer implementation for RmlUI.
 *
 * Overrides the slim UIRenderer interface (BeginFrame/EndFrame/NeedsOffscreen)
 * and exposes RmlUi-specific methods (CompileGeometry, DrawGeometry, SetScissor,
 * EnableClipMask, etc.) as plain public methods — not on the abstract interface.
 *
 * Owns the GPU resources needed to draw RmlUI geometry:
 * - 7 PSOs (textured, untextured, stencil variants, composite)
 * - Vertex/index buffer ring allocator for compiled geometry
 * - Texture cache (SRB per texture)
 * - Push constant management
 * - Scissor and stencil state tracking
 */
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RmlUIRenderer, public UIRenderer)

#pragma region METHOD
public:
	RmlUIRenderer() MYDEFAULT;
	VIRTUAL ~RmlUIRenderer();

	/// Initialize GPU resources (shaders, PSOs). Must be called after RHI is ready.
	void METHOD(Initialize)(RHI::CommandList* cmd_list, RHI::Texture* backbuffer_rtv, RHI::Texture* backbuffer_dsv, UInt32 viewport_w, UInt32 viewport_h);
	/// Set viewport reference for resize-safe size query in BeginFrame.
	void METHOD(SetViewport)(RHI::Viewport* vp) { m_viewport = vp; }
	/// Release all GPU resources.
	void METHOD(Shutdown)();

	// === UIRenderer interface ===
	VIRTUAL void METHOD(BeginFrame)(RHI::CommandList* cmd) OVERRIDE;
	VIRTUAL void METHOD(EndFrame)(RHI::CommandList* cmd) OVERRIDE;
	VIRTUAL bool METHOD(NeedsOffscreen)() CONST OVERRIDE;
	VIRTUAL UInt32 METHOD(GetViewportHeight)() CONST OVERRIDE;

	// === RmlUi-specific methods (non-virtual, not on UIRenderer) ===

	/// Compile vertex/index data into a GPU-resident geometry handle.
	UIGeometryHandle METHOD(CompileGeometry)(
		CONST void* vertices, UInt32 vtx_count, UInt32 vtx_stride,
		CONST void* indices, UInt32 idx_count, bool idx32);

	void METHOD(DrawGeometry)(UIGeometryHandle geo, CONST void* transform, UITextureHandle tex);
	void METHOD(ReleaseGeometry)(UIGeometryHandle geo);

	UITextureHandle METHOD(CreateTexture)(CONST void* pixel_data, UInt32 w, UInt32 h);
	void METHOD(ReleaseTexture)(UITextureHandle tex);

	/// Enable or disable scissor testing.
	void METHOD(EnableScissor)(bool enable);
	/// Set scissor rectangle (window coords: origin top-left, Y-down).
	void METHOD(SetScissor)(Int x, Int y, UInt32 w, UInt32 h);

	void METHOD(EnableClipMask)(bool enable);
	void METHOD(RenderToClipMask)(Int operation, UIGeometryHandle geo, CONST void* transform);
	void METHOD(SetTransform)(CONST void* transform);
	void METHOD(SetTranslation)(Float32 x, Float32 y);

	/// Composite an offscreen UI layer onto the backbuffer (Mode B).
	void METHOD(CompositeLayer)(RHI::Texture* ui_layer_texture, UInt32 tex_width, UInt32 tex_height);

protected:
private:
	// Shaders
	RHI::Shader* m_vs = nullptr;
	RHI::Shader* m_fs_textured = nullptr;
	RHI::Shader* m_fs_composite = nullptr;

	// Pipeline states
	RHI::RenderPipelineState* m_pso_textured = nullptr;
	RHI::RenderPipelineState* m_pso_composite = nullptr;
	RHI::RenderPipelineState* m_pso_untextured = nullptr;
	RHI::RenderPipelineState* m_pso_stencil_set = nullptr;
	RHI::RenderPipelineState* m_pso_stencil_intersect = nullptr;
	RHI::RenderPipelineState* m_pso_textured_stencil = nullptr;
	RHI::RenderPipelineState* m_pso_untextured_stencil = nullptr;

	// SRB for untextured draw
	RHI::ShaderResourceBinding* m_srb_untextured = nullptr;

	// Per-draw uniform buffer (SSBO, Storage|Dynamic)
	RHI::Buffer* m_per_draw_buf = nullptr;
	struct PerDrawData { float transform[16]; float translation[2]; };

	// Current command list (set by BeginFrame)
	RHI::CommandList* m_current_cmd = nullptr;

	// Viewport dimensions
	UInt32 m_viewport_w = 0;
	UInt32 m_viewport_h = 0;

	// Geometry handle mapping
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

	// Texture handle mapping
	struct TexSlot {
		RHI::Texture* texture = nullptr;
		RHI::ShaderResourceBinding* srb = nullptr;
	};
	Map<GenericHandle, TexSlot> m_textures;
	Vector<RHI::Texture*> m_pending_transitions;
	UInt32 m_next_tex_handle = 1;

	// Cached per-draw data
	float m_transform[16];
	float m_translation[2];

	// Scissor state
	bool m_scissor_enabled = false;
	Int m_scissor_x = 0, m_scissor_y = 0;
	UInt32 m_scissor_w = 0, m_scissor_h = 0;

	// Clip mask state
	bool m_clip_mask_enabled = false;

	// Stats
	UIDrawStats m_stats;

	// Internal helpers
	void METHOD(CreatePSOs)();
	void METHOD(CreateShaders)(RHI::CommandList* cmd_list);
	void METHOD(DestroyPSOs)();
	void METHOD(DestroyShaders)();
	void METHOD(UploadPerDrawData)();
	RHI::RenderPipelineState* METHOD(SelectPSO)(bool has_texture) CONST;
#pragma endregion

#pragma region MEMBER
public:
protected:
private:
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE // RmlUI
MYRENDERER_END_NAMESPACE // UI
MYRENDERER_END_NAMESPACE // MXRender

#endif // !_RMLUIRENDERER_
