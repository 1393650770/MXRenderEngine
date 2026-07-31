#pragma once
#ifndef _PIXEL_WORLD_RENDERER_
#define _PIXEL_WORLD_RENDERER_

#include "Core/ConstDefine.h"
#include "World/PixelWorldConstants.h"
#include "RHI/RenderResource.h"   // TextureDesc (for RenderGraphResource template arg)

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Buffer;
class RenderPipelineState;
class ShaderResourceBinding;
class CommandList;
class Texture;
MYRENDERER_END_NAMESPACE
MYRENDERER_BEGIN_NAMESPACE(Render)
class RenderGraph;
class RenderGraphPassBuilder;
template<typename Desc, typename Actual> class RenderGraphResource;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)
class PixelWorld;
class MaterialRegistry;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// MVC-View for the pixel world. Owns the GPU display mirror and palette
// buffers plus the display pass. Read-only consumer of simulator state.
class PixelWorldRenderer
{
#pragma region METHOD
public:
	PixelWorldRenderer();
	~PixelWorldRenderer();

	void METHOD(Init)();
	void METHOD(Shutdown)();

	// Registers the display pass on the render graph (write to backbuffer).
	void METHOD(RegisterDisplayPass)(
		Render::RenderGraph* graph,
		Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>* backbuffer_resource,
		RHI::CommandList* immediate_cmd);

	// Full mirror upload from the CPU world (Phase 1; Phase 2 switches to the
	// GPU state buffer via CreateDisplayBindings + SetDisplaySource).
	void METHOD(UploadMirror)(CONST PixelWorld& world);

	// Palette upload from the material registry (called once at init, and
	// again when new materials register).
	void METHOD(UploadPalette)();

	// Phase 2: pre-bind a second display source (GPU authoritative state_a).
	void METHOD(CreateDisplayBindings)(RHI::Buffer* gpu_state);
	// Runtime source switch (SRB selection only - never re-binds descriptors).
	void METHOD(SetDisplaySource)(Bool use_gpu);

protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:
	RHI::Buffer* mirror_ = nullptr;          // Storage|Dynamic, W*H*4 bytes
	RHI::Buffer* palette_ = nullptr;         // Storage|Dynamic, kMaxMaterials*16
	RHI::Buffer* params_ = nullptr;          // Storage|Dynamic, {world_w, world_h}
	RHI::RenderPipelineState* pso_display_ = nullptr;
	RHI::ShaderResourceBinding* srb_mirror_ = nullptr;   // binds mirror_ + palette_
	RHI::ShaderResourceBinding* srb_gpu_ = nullptr;      // binds gpu_state_ + palette_
	Bool use_gpu_source_ = false;

private:
#pragma endregion
};  // class PixelWorldRenderer

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _PIXEL_WORLD_RENDERER_