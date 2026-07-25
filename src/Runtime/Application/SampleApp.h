#pragma once
#ifndef _SAMPLEAPP_
#define _SAMPLEAPP_
#include "Core/ConstDefine.h"
#include "Render/RenderInterface.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
class PlatformWindow;
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Texture;
class Viewport;
class CommandList;
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(Application)

// Intermediate base for sample apps. Collapses the boilerplate every sample
// used to copy: the 8-line main(), BackBuffer/DepthStencil retained-resource
// registration, the SetRenderTarget+clear-value block, and the RDG-to-JSON
// serialization. Subclasses implement OnInitScene() (add passes on `graph`)
// and optionally OnShutdownScene()/OnUpdate()/OnRender().
MYRENDERER_BEGIN_CLASS_WITH_DERIVE(SampleApp, public MXRender::RenderInterface)

#pragma region METHOD
public:
	SampleApp() MYDEFAULT;
	VIRTUAL ~SampleApp() MYDEFAULT;

	static Int METHOD(RunSample)(SampleApp& in_app, CONST String& in_title = "MXRender");
	static void METHOD(SetPlatformData)(void* data);
	static void* METHOD(GetPlatformData)() { return s_platform_data; }
	static void* s_platform_data;

	// ==== RenderInterface (template methods) ====
	VIRTUAL void METHOD(OnInit_Logic)(PlatformWindow* in_window, RHI::Viewport* in_viewport) OVERRIDE FINAL;
	VIRTUAL void METHOD(OnShutdown_Logic)() OVERRIDE FINAL;
	VIRTUAL void METHOD(OnRender)() OVERRIDE;

	// ==== Subclass hooks ====
	VIRTUAL void METHOD(OnInitScene)() PURE;
	VIRTUAL void METHOD(OnShutdownScene)() {}

	// ==== Boilerplate helpers ====
	PlatformWindow* METHOD(GetPlatformWindow)() CONST { return platform_window; }
	RHI::Texture* METHOD(GetBackBuffer)() CONST;
	RHI::Texture* METHOD(GetDepthStencil)() CONST;
	Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>* METHOD(GetBackBufferResource)() CONST { return backbuffer_resource; }
	Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>* METHOD(GetDepthStencilResource)() CONST { return depth_stencil_resource; }
	void METHOD(BindBackBufferTarget)(RHI::CommandList* in_cmd) CONST;
	void METHOD(SaveGraphDefinition)(CONST String& in_graph_name, CONST String& in_file_path);
	UInt32 METHOD(GetViewportWidth)() CONST;
	UInt32 METHOD(GetViewportHeight)() CONST;
protected:

private:

#pragma endregion

#pragma region MEMBER
public:

protected:
	PlatformWindow* platform_window = nullptr;
	Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>* backbuffer_resource = nullptr;
	Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>* depth_stencil_resource = nullptr;
	Bool auto_compile = true;
	RHI::Viewport* viewport = nullptr;
private:

#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif
