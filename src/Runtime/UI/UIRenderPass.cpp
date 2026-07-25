#include "UIRenderPass.h"
#include "UIRenderer.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderTexture.h"
#include "Render/Core/RenderGraphPass.h"
#include "Render/Core/RenderGraphResource.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)

void RegisterUIPass(
	Render::RenderGraph* graph,
	Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>* bb_resource,
	UIRenderer* renderer,
	RHI::CommandList* cmd_list,
	UInt32 viewport_w, UInt32 viewport_h,
	std::function<void(RHI::CommandList*)> draw_fn)
{
	if (!graph || !bb_resource || !renderer) return;

	bool needs_offscreen = renderer->NeedsOffscreen();

	if (!needs_offscreen)
	{
		// === Mode A: direct backbuffer rendering ===
		graph->AddRenderPass<UIPassData>("RmlUIPass", graph, cmd_list,
			[bb_resource](UIPassData& data, Render::RenderGraphPassBuilder& builder, RHI::CommandList* in_cmd_list)
		{
			builder.Write(bb_resource, ENUM_RESOURCE_STATE::RenderTarget);
		},
			[draw_fn](CONST UIPassData& data, RHI::CommandList* in_cmd_list)
		{
			if (draw_fn) draw_fn(in_cmd_list);
		});
	}
	else
	{
		// === Mode B: offscreen + composite ===
		RHI::TextureDesc color_desc;
		color_desc.width = viewport_w;
		color_desc.height = viewport_h;
		color_desc.mip_level = 1;
		color_desc.layer_count = 1;
		color_desc.format = ENUM_TEXTURE_FORMAT::RGBA8;
		color_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
		color_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_COLOR_ATTACHMENT
			| ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
		color_desc.resource_state = ENUM_RESOURCE_STATE::Undefined;
		color_desc.clear_value.color[0] = 0.0f;
		color_desc.clear_value.color[1] = 0.0f;
		color_desc.clear_value.color[2] = 0.0f;
		color_desc.clear_value.color[3] = 0.0f;

		RHI::TextureDesc ds_desc;
		ds_desc.width = viewport_w;
		ds_desc.height = viewport_h;
		ds_desc.mip_level = 1;
		ds_desc.layer_count = 1;
		ds_desc.format = ENUM_TEXTURE_FORMAT::D24S8;
		ds_desc.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
		ds_desc.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_DEPTH_ATTACHMENT;
		ds_desc.resource_state = ENUM_RESOURCE_STATE::Undefined;
		ds_desc.clear_value.ds_value[0] = 1.0f;
		ds_desc.clear_value.ds_value[1] = 0.0f;

		Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>* ui_color = nullptr;
		Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>* ui_ds = nullptr;

		graph->AddRenderPass<UIPassData>("RmlUIOffscreenPass", graph, cmd_list,
			[&ui_color, &ui_ds, color_desc, ds_desc](UIPassData& data, Render::RenderGraphPassBuilder& builder, RHI::CommandList* in_cmd_list)
		{
			ui_color = builder.Create<Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>>("RmlUI_Color", color_desc);
			ui_ds = builder.Create<Render::RenderGraphResource<RHI::TextureDesc, RHI::Texture>>("RmlUI_DS", ds_desc);
			builder.Write(ui_color, ENUM_RESOURCE_STATE::RenderTarget);
			builder.Write(ui_ds, ENUM_RESOURCE_STATE::DepthWrite);
		},
			[renderer, draw_fn, &ui_color, &ui_ds](CONST UIPassData& data, RHI::CommandList* in_cmd_list)
		{
			Vector<RHI::Texture*> rtvs = { ui_color->GetActual() };
			Vector<RHI::ClearValue> cvs;
			RHI::ClearValue c0, c1;
			c0.color[0] = 0.0f; c0.color[1] = 0.0f; c0.color[2] = 0.0f; c0.color[3] = 0.0f;
			c1.ds_value[0] = 1.0f; c1.ds_value[1] = 0.0f;
			cvs.push_back(c0); cvs.push_back(c1);
			in_cmd_list->SetRenderTarget(rtvs, ui_ds->GetActual(), cvs, true);
			renderer->BeginFrame(in_cmd_list);
			if (draw_fn) draw_fn(in_cmd_list);
			renderer->EndFrame(in_cmd_list);
		});

		graph->AddRenderPass<UIPassData>("RmlUICompositePass", graph, cmd_list,
			[bb_resource, &ui_color](UIPassData& data, Render::RenderGraphPassBuilder& builder, RHI::CommandList* in_cmd_list)
		{
			builder.Read(ui_color, ENUM_RESOURCE_STATE::ShaderResource);
			builder.Write(bb_resource, ENUM_RESOURCE_STATE::RenderTarget);
		},
			[](CONST UIPassData& data, RHI::CommandList* in_cmd_list)
		{
			// Composite pass placeholder for Phase 3
		});
	}
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
