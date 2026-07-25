#pragma once


#ifndef _RENDERROURCE_
#define _RENDERROURCE_
#include "RHI/RenderEnum.h"
#include <cstdint>
#include <memory>
#include <string>
#include "Core/BaseObject.h"
#include <algorithm>
#include <utility>
#include "Core/ConstDefine.h"



#define MYRENDER_MAX_RENDER_TARGETS 8

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class Texture;
class Shader;
class RenderPass;

MYRENDERER_BEGIN_STRUCT(BufferDesc)
public:
	UInt32 size = 0;
	UInt32 stride = 0;
	ENUM_BUFFER_TYPE type = ENUM_BUFFER_TYPE::None;
	BufferDesc() = default;
	BufferDesc(const BufferDesc& other)
	{
		*this = other;
	}

	BufferDesc& operator=(const BufferDesc& other)
	{
		size = other.size;
		stride = other.stride;
		type = other.type;
		return *this;
	}

MYRENDERER_END_STRUCT


MYRENDERER_BEGIN_STRUCT(ShaderDataPayload)
public:
	ShaderDataPayload() MYDEFAULT;
	ShaderDataPayload(CONST ShaderDataPayload& other);

	ShaderDataPayload& operator=(CONST ShaderDataPayload& other)
	{
		return *this;
	}
	~ShaderDataPayload()
	{
		data.clear();
	}
	MYRENDERER_BEGIN_STRUCT(ShaderBindingOverrides)
		String name = "";
		ENUM_BINDING_RESOURCE_TYPE overriden_type = ENUM_BINDING_RESOURCE_TYPE::Invalid;
	MYRENDERER_END_STRUCT
	Vector<ShaderBindingOverrides> shader_binding_overrides;
	Vector<UInt32> data;
	// WebGPU path: WGSL source text (UTF-8). Empty on Vulkan/Windows builds.
	// Filled by ShaderLibrary::ReadWgsl on PLATFORM_WGPU; ignored by VK backend.
	String wgsl_source;
MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_STRUCT(ShaderDesc)
public:
	ShaderDesc() MYDEFAULT;
	ShaderDesc(ENUM_SHADER_STAGE in_shader_type) : shader_type(in_shader_type) {}
	ENUM_SHADER_STAGE shader_type = ENUM_SHADER_STAGE::Invalid;
	String debug_name;
	String entry_name = "main";
	String shader_name;
	Vector<UInt32> spirv_data;  // Merged from ShaderDataPayload for self-contained RDG resource

MYRENDERER_END_STRUCT


MYRENDERER_BEGIN_STRUCT(FrameBufferDesc)
	UInt32 width = 0;
	UInt32 height = 0;
	UInt8 layer = 1;
	//last element is the depth stencil attachment
	Vector<RHI::Texture*> render_targets;
	RHI::Texture* depth_stencil_view = nullptr;
MYRENDERER_END_STRUCT



MYRENDERER_BEGIN_STRUCT(RenderPassDesc)
public:

	MYRENDERER_BEGIN_STRUCT(RenderPassAttatchmentDesc)
	public:
		ENUM_TEXTURE_FORMAT 			format           ;
		UInt8                   sample_count     =1;
		ENUM_RENDERPASS_ATTACHMENT_LOAD_OP      load_op          ;
		ENUM_RENDERPASS_ATTACHMENT_STORE_OP     store_op;
		ENUM_RENDERPASS_ATTACHMENT_LOAD_OP      stencil_load_op  ;
		ENUM_RENDERPASS_ATTACHMENT_STORE_OP     stencil_store_op;
		ENUM_RESOURCE_STATE          initial_state ;
		ENUM_RESOURCE_STATE          final_state;
	MYRENDERER_END_STRUCT

	MYRENDERER_BEGIN_STRUCT(RenderPassAttachmentReferenceDesc)
	public:
		UInt8 attachment_index = 0;
		ENUM_RESOURCE_STATE state = ENUM_RESOURCE_STATE::Invalid;
	MYRENDERER_END_STRUCT

	Vector<RenderPassAttatchmentDesc> attachments;
	Vector<RenderPassAttachmentReferenceDesc> attachment_refs;
	Bool last_attachment_is_depth_stencil = true;
MYRENDERER_END_STRUCT


static constexpr UInt32 g_max_render_targets = 8;
static constexpr UInt32 g_max_viewports = 16;
static constexpr UInt32 g_max_vertex_attributes = 16;
static constexpr UInt32 g_max_binding_layouts = 5;
static constexpr UInt32 g_max_bindings_per_layout = 128;
static constexpr UInt32 g_max_volatile_constant_buffers_per_layout = 6;
static constexpr UInt32 g_max_volatile_constant_buffers = 32;
static constexpr UInt32 g_max_push_constant_size = 128; // D3D12: root signature is 256 bytes max., Vulkan: 128 bytes of push constants guaranteed
static constexpr UInt32 c_ConstantBufferOffsetSizeAlignment = 256;

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RenderResource,public IObject)
public:
	RenderResource() MYDEFAULT;
	VIRTUAL ~RenderResource() MYDEFAULT;
	VIRTUAL void METHOD(Release)();
	VIRTUAL void METHOD(AddRef)() ;
	VIRTUAL void METHOD(Realize)() CONST;  
	VIRTUAL void METHOD(DeRealize)() CONST;
protected:
	Bool is_valid{ false };
	UInt32 ref_count{ 1 };

private:


protected:

MYRENDERER_END_CLASS


MYRENDERER_BEGIN_CLASS(ShaderData)

public:
	Vector<UInt32> spirv;
	ENUM_SHADER_STAGE stage{ ENUM_SHADER_STAGE::Invalid };

	ShaderData() MYDEFAULT;
	~ShaderData()
	{
		spirv.clear();
	}
	Bool is_valid() CONST { return spirv.size() > 0; }
MYRENDERER_END_CLASS


MYRENDERER_BEGIN_STRUCT(MipmapInfo)
public:
	size_t dataSize;
	size_t dataOffset;
MYRENDERER_END_STRUCT

union ClearValue
{
	Float32 color[4];
	Float32 ds_value[2];
};

MYRENDERER_BEGIN_STRUCT(TextureDesc)
public:
	TextureDesc() MYDEFAULT;
	TextureDesc(CONST TextureDesc& other);

	TextureDesc& operator=(CONST TextureDesc& other)
	{
		width = other.width;
		height = other.height;
		mip_level = other.mip_level;
		layer_count = other.layer_count;
		format = other.format;
		type = other.type;
		depth = other.depth;
		samples = other.samples;
		clear_value = other.clear_value;
		usage = other.usage;
		resource_state = other.resource_state;
		return *this;
	}

	UInt32 width = 0;
	UInt32 height = 0;
	UInt8 mip_level = 1;
	UInt8 layer_count = 1;
	ENUM_TEXTURE_FORMAT format = ENUM_TEXTURE_FORMAT::None;
	ENUM_TEXTURE_TYPE type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	ENUM_TEXTURE_USAGE_TYPE usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_NOT_VALID;
	ENUM_RESOURCE_STATE  resource_state = ENUM_RESOURCE_STATE::Undefined;
	UInt16 depth = 1;
	UInt8 samples = 1;

	ClearValue clear_value;

MYRENDERER_END_STRUCT



MYRENDERER_BEGIN_STRUCT(TextureDataPayload)
public:
	TextureDataPayload() MYDEFAULT;
	TextureDataPayload(CONST TextureDataPayload& other);

	TextureDataPayload& operator=(CONST TextureDataPayload& other)
	{
		data = other.data;
		mip_level = other.mip_level;
		layer_count = other.layer_count;
		format = other.format;
		type = other.type;
		return *this;
	}

	Vector<Char> data;
	UInt8 mip_level = 1;
	UInt8 layer_count = 1;
	UInt32 width = 0;
	UInt32 height = 0;
	UInt32 depth = 1;
	ENUM_TEXTURE_FORMAT format = ENUM_TEXTURE_FORMAT::None;
	ENUM_TEXTURE_TYPE type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
	MYRENDERER_BEGIN_STRUCT(MipLevelProperties)
	public:
		UInt32 logical_width = 0;
		UInt32 logical_height = 0;
		UInt32 storage_width = 0;
		UInt32 storage_height = 0;
		UInt32 depth = 1;
		UInt64 row_size = 0;
		UInt64 slice_size = 0;
		UInt64 mip_size = 0;
	MYRENDERER_END_STRUCT

	MipLevelProperties METHOD(GetMipLevelProperties)(UInt8 in_mip_level) CONST;

MYRENDERER_END_STRUCT


MYRENDERER_BEGIN_STRUCT(BlenderState)

	MYRENDERER_BEGIN_STRUCT(RenderTarget)
	public:
		Bool        blend_enable = false;
		ENUM_BLEND_FACTOR src_color{ ENUM_BLEND_FACTOR::ENUM_ONE };
		ENUM_BLEND_FACTOR dst_color{ ENUM_BLEND_FACTOR::ENUM_ZERO };
		ENUM_BLEND_FACTOR src_alpha{ ENUM_BLEND_FACTOR::ENUM_ONE };
		ENUM_BLEND_FACTOR dst_alpha{ ENUM_BLEND_FACTOR::ENUM_ZERO };
		ENUM_BLEND_EQUATION op_color{ ENUM_BLEND_EQUATION::ENUM_ADD };
		ENUM_BLEND_EQUATION op_alpha{ ENUM_BLEND_EQUATION::ENUM_ADD };
		ENUM_COLOR_MASK write_mask{ ENUM_COLOR_MASK::All };

		RenderTarget() MYDEFAULT;
		~RenderTarget() MYDEFAULT;

		constexpr Bool operator == (CONST RenderTarget& rhs) CONST
		{
			return  blend_enable == rhs.blend_enable &&
				src_color == rhs.src_color &&
				dst_color == rhs.dst_color &&
				src_alpha == rhs.src_alpha &&
				dst_alpha == rhs.dst_alpha &&
				op_color == rhs.op_color &&
				op_alpha == rhs.op_alpha &&
				write_mask == rhs.write_mask;
		};

		constexpr Bool operator != (CONST RenderTarget& rhs) CONST
		{
			return !(*this == rhs);
		};


	MYRENDERER_END_STRUCT

	Vector<RenderTarget> render_targets;
	Bool enable_alpha_to_coverage = false;

	constexpr Bool operator == (CONST BlenderState& rhs) CONST
	{
		if (enable_alpha_to_coverage != rhs.enable_alpha_to_coverage || render_targets.size()!= rhs.render_targets.size())
			return false;

		for (UInt32 i = 0; i < render_targets.size(); ++i)
		{
			if (render_targets[i] != rhs.render_targets[i])
			{
				return false;
			}
		}
		return true;
	};

	constexpr Bool operator != (CONST BlenderState& rhs) CONST
	{
		return !(*this == rhs);
	};
MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_STRUCT(DepthStencilState)
public:
	DepthStencilState()MYDEFAULT;
	~DepthStencilState()MYDEFAULT;
	
	Bool depth_test_enable{ true };
	Bool depth_write_enable{ true };
	ENUM_STENCIL_FUNCTION depth_func{ ENUM_STENCIL_FUNCTION::ENUM_ALWAYS };
	Bool stencil_test_enable{ false };
	UInt8 stencil_read_mask{ 0xFF };
	UInt8 stencil_write_mask{ 0xFF };
	UInt8 stencil_ref{ 0 };
	StencilOpDesc front_face_stencil{};
	StencilOpDesc back_face_stencil{};
	Bool depth_bounds_test_enable{ false };
	Float32 min_depth_bounds { 0.0f };
	Float32 max_depth_bounds { 0.0f };

	constexpr Bool operator == (CONST DepthStencilState& rhs) CONST
	{
		return depth_test_enable == rhs.depth_test_enable &&
			depth_write_enable == rhs.depth_write_enable &&
			depth_func == rhs.depth_func &&
			stencil_test_enable == rhs.stencil_test_enable &&
			stencil_read_mask == rhs.stencil_read_mask &&
			stencil_write_mask == rhs.stencil_write_mask &&
			stencil_ref == rhs.stencil_ref &&
			front_face_stencil == rhs.front_face_stencil &&
			back_face_stencil == rhs.back_face_stencil &&
			depth_bounds_test_enable == rhs.depth_bounds_test_enable &&
			min_depth_bounds == rhs.min_depth_bounds &&
			max_depth_bounds == rhs.max_depth_bounds;
	};

MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_STRUCT(RasterizerState)
	RasterizerState()MYDEFAULT;
	~RasterizerState()MYDEFAULT;

	ENUM_RASTER_CULLMODE cull_mode{ ENUM_RASTER_CULLMODE::Back };
	ENUM_RASTER_FILLMODE fill_mode{ ENUM_RASTER_FILLMODE::Solid };
	Bool front_counter_clockwise{ false };
	Bool depth_clip_enable{ false };
	Bool scissor_enable{ false };
	Bool antialiased_line_enable{ false };
	Float32 depth_bias{ 0.0f };
	Float32 depth_bias_clamp{ 0.0f };
	Float32 depth_bias_slope_scaled{ 0.0f };
	Float32 line_width{ 1.0f };
	UInt8 sample_count { 1 };

	constexpr Bool operator == (CONST RasterizerState& rhs) CONST
	{
		return cull_mode == rhs.cull_mode &&
			fill_mode == rhs.fill_mode &&
			front_counter_clockwise == rhs.front_counter_clockwise &&
			depth_clip_enable == rhs.depth_clip_enable &&
			scissor_enable == rhs.scissor_enable &&
			antialiased_line_enable == rhs.antialiased_line_enable &&
			depth_bias == rhs.depth_bias &&
			depth_bias_clamp == rhs.depth_bias_clamp &&
			depth_bias_slope_scaled == rhs.depth_bias_slope_scaled &&
			line_width == rhs.line_width;
	};

	constexpr Bool operator != (CONST RasterizerState& rhs) CONST
	{
		return !(*this == rhs);
	};

MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_STRUCT(VertexInputLayout)
public:
	UInt32 binding{ 0 };
	UInt32 location{ 0 };
	ENUM_TEXTURE_FORMAT attribute_format{ ENUM_TEXTURE_FORMAT::None };
	ENUM_VERTEX_INPUTRATE input_rate{ ENUM_VERTEX_INPUTRATE::PerVertex };
	UInt32 offset{ 0 };

	VertexInputLayout() MYDEFAULT;
	~VertexInputLayout() MYDEFAULT;

	VertexInputLayout& operator= (CONST VertexInputLayout& rhs)
	{
		if (this != &rhs) // �Ը�ֵ���
		{
			binding = rhs.binding;
			location = rhs.location;
			attribute_format = rhs.attribute_format;
			input_rate = rhs.input_rate;
			offset = rhs.offset;
		}
		return *this;
	}
	VertexInputLayout(CONST VertexInputLayout& other)
	{
		*this = other;
	}

	VertexInputLayout(VertexInputLayout&& other) noexcept
		: binding(std::exchange(other.binding, 0)),
		location(std::exchange(other.location, 0)),
		attribute_format(std::exchange(other.attribute_format, ENUM_TEXTURE_FORMAT::None)),
		input_rate(std::exchange(other.input_rate, ENUM_VERTEX_INPUTRATE::PerVertex)),
		offset(std::exchange(other.offset, 0))
	{
	}

	Bool operator== (CONST VertexInputLayout& rhs) CONST
	{
		return binding == rhs.binding &&
			location == rhs.location &&
			attribute_format == rhs.attribute_format &&
			offset == rhs.offset &&
			input_rate == rhs.input_rate;
	};


MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(RenderGraphiPipelineStateDesc,public RenderResource)
public:
	RenderGraphiPipelineStateDesc() MYDEFAULT;
	VIRTUAL ~RenderGraphiPipelineStateDesc() MYDEFAULT;

	Shader* shaders[ENUM_SHADER_STAGE::NumStages]{nullptr};
	ENUM_PRIMITIVE_TYPE primitive_topology{ ENUM_PRIMITIVE_TYPE::TriangleList };
	// Tessellation: only used when primitive_topology == PatchList; 0 is treated as 3
	UInt32 patch_control_points = 0;
	BlenderState blend_state;
	DepthStencilState depth_stencil_state;
	RasterizerState raster_state;
	Vector<VertexInputLayout> vertex_input_layout;
	Vector<RHI::Texture*> render_targets;
	RHI::Texture* depth_stencil_view = nullptr;
	constexpr Bool operator == (CONST RenderGraphiPipelineStateDesc& rhs) CONST
	{
		for (UInt32 i = 0; i < ENUM_SHADER_STAGE::NumStages; ++i)
		{
			if (shaders[i] != rhs.shaders[i])
				return false;
		}
		if(vertex_input_layout.size() != rhs.vertex_input_layout.size())
			return false;
		for (UInt32 i = 0; i < vertex_input_layout.size(); ++i)
		{
			if (vertex_input_layout[i] != rhs.vertex_input_layout[i])
				return false;
		}
		return
			primitive_topology == rhs.primitive_topology &&
			patch_control_points == rhs.patch_control_points &&
			blend_state == rhs.blend_state &&
			depth_stencil_state == rhs.depth_stencil_state &&
			raster_state == rhs.raster_state;
	};

	constexpr Bool operator != (CONST RenderGraphiPipelineStateDesc& rhs) CONST
	{
		return !(*this == rhs);
	};

	Vector<VertexInputLayout> GetSortedVertexInputLayout() CONST
	{
		Vector<VertexInputLayout> sortedLayouts = vertex_input_layout;
		std::sort(sortedLayouts.begin(), sortedLayouts.end(), [](CONST VertexInputLayout& lhs, CONST VertexInputLayout& rhs)->Bool {
			if (lhs.input_rate != rhs.input_rate)
				return lhs.input_rate < rhs.input_rate;
			else if (lhs.binding != rhs.binding)
				return lhs.binding < rhs.binding;
			return lhs.location < rhs.location;
		});
		return sortedLayouts;
	}

	UInt64 GetHash() CONST;
private:
	mutable UInt64 hash = 0;
MYRENDERER_END_CLASS


// --  
// Lightweight compute pipeline descriptor - only needs a compute shader,
// no graphics state (blend/raster/depth/vertex input)
MYRENDERER_BEGIN_STRUCT(ComputePipelineStateDesc)
public:
    Shader* compute_shader = nullptr;

    constexpr Bool operator == (CONST ComputePipelineStateDesc& rhs) CONST
    {
        return compute_shader == rhs.compute_shader;
    }
    constexpr Bool operator != (CONST ComputePipelineStateDesc& rhs) CONST
    {
        return !(*this == rhs);
    }
MYRENDERER_END_STRUCT
// --  


MYRENDERER_BEGIN_STRUCT(RenderPassCacheKey)
	public:
		RenderPassCacheKey() {}
		RenderPassCacheKey(UInt8 in_num_render_targets, CONST ENUM_TEXTURE_FORMAT* in_render_target_formats, ENUM_TEXTURE_FORMAT in_depth_stencil_format, UInt8 in_sample_count, Bool in_is_enable_vrs, Bool in_is_read_only_dsv);

		Bool operator == (CONST RenderPassCacheKey& rhs) CONST
		{
			return GetHash() == rhs.GetHash() &&
				num_render_targets == rhs.num_render_targets &&
				depth_stencil_format == rhs.depth_stencil_format &&
				sample_count == rhs.sample_count &&
				is_enable_vrs == rhs.is_enable_vrs &&
				is_read_only_dsv == rhs.is_read_only_dsv &&
				memcmp(render_target_formats, rhs.render_target_formats, sizeof(render_target_formats)) == 0;
		}
		UInt64 METHOD(GetHash)() CONST;

		UInt32 num_render_targets = 0;
		UInt32 sample_count = 1;
		Bool is_enable_vrs = false;
		Bool is_read_only_dsv = false;
		ENUM_TEXTURE_FORMAT depth_stencil_format = ENUM_TEXTURE_FORMAT::None;
		ENUM_TEXTURE_FORMAT render_target_formats[MYRENDER_MAX_RENDER_TARGETS] = { ENUM_TEXTURE_FORMAT::None };
	private:
		mutable UInt64 hash = 0;
MYRENDERER_END_STRUCT



MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE


#endif _RENDERROURCE_
