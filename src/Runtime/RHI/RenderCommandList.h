#pragma once


#ifndef _RENDERCOMMANDLIST_
#define _RENDERCOMMANDLIST_
#include "Core/ConstDefine.h"
#include "RenderResource.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)

MYRENDERER_BEGIN_STRUCT(DrawAttribute)
public:
	UInt32                                    vertexCount = 0;
	UInt32                                    instanceCount = 0;
	UInt32                                    firstVertex = 0;
	UInt32                                    firstInstance = 0;
MYRENDERER_END_STRUCT

// ---- Indirect argument structs ----
// Field layout matches the Vulkan indirect command structs one-to-one so a
// CPU-filled (or compute-written) args buffer can be consumed directly.

MYRENDERER_BEGIN_STRUCT(DrawIndirectArgs)          // = VkDrawIndirectCommand
public:
	UInt32                                    vertex_count = 0;
	UInt32                                    instance_count = 0;
	UInt32                                    first_vertex = 0;
	UInt32                                    first_instance = 0;
MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_STRUCT(DrawIndexedIndirectArgs)   // = VkDrawIndexedIndirectCommand
public:
	UInt32                                    index_count = 0;
	UInt32                                    instance_count = 0;
	UInt32                                    first_index = 0;
	Int                                       vertex_offset = 0;
	UInt32                                    first_instance = 0;
MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_STRUCT(DispatchIndirectArgs)      // = VkDispatchIndirectCommand
public:
	UInt32                                    group_x = 0;
	UInt32                                    group_y = 0;
	UInt32                                    group_z = 0;
MYRENDERER_END_STRUCT

MYRENDERER_BEGIN_NAMESPACE(RHI)
class RenderPipelineState;
class ShaderResourceBinding;

// ---- RHI Command recording ----
// When enable_rhi_thread=true, commands are recorded into structs
// and replayed on the RHI thread. When bypass=true (default),
// commands execute immediately on the calling thread.

enum class RHICommandType : UInt8
{
	SetGraphicsPipeline,
	SetComputePipeline,
	SetRenderTarget,
	SetSRB,
	Draw,
	Dispatch,
	TransitionTexture,
	ClearTexture,
	ResourceBarrier,
	FlushBarriers,
	SetPushConstants,
	MapBuffer,
	UnmapBuffer,
	CopyBuffer,
	CopyBufferToImage,
	Begin,
	End,
	BeginRenderPass,
	EndRenderPass,
	BeginUI,
	EndUI,
	WriteTimestamp,
	RenderImGui,
	SetVertexBuffer,
	SetIndexBuffer,
	DrawIndexed,
	DrawIndirect,
	DrawIndexedIndirect,
	DispatchIndirect,
	SetScissorEnable,
	SetScissor,
	UpdateBuffer,
};

struct RHICommand
{
	RHICommandType type;
	RHICommand(RHICommandType t) : type(t) {}
	virtual ~RHICommand() MYDEFAULT;
};

// ---- Command structs for recording ----
struct RHICmdDraw : RHICommand {
	DrawAttribute attr;
	RHICmdDraw(CONST DrawAttribute& a) : RHICommand(RHICommandType::Draw), attr(a) {}
};
struct RHICmdDispatch : RHICommand {
	UInt32 gx, gy, gz;
	RHICmdDispatch(UInt32 x, UInt32 y, UInt32 z) : RHICommand(RHICommandType::Dispatch), gx(x), gy(y), gz(z) {}
};
struct RHICmdSetGraphicsPipeline : RHICommand {
	RenderPipelineState* pso;
	RHICmdSetGraphicsPipeline(RenderPipelineState* p) : RHICommand(RHICommandType::SetGraphicsPipeline), pso(p) {}
};
struct RHICmdSetComputePipeline : RHICommand {
	RenderPipelineState* pso;
	RHICmdSetComputePipeline(RenderPipelineState* p) : RHICommand(RHICommandType::SetComputePipeline), pso(p) {}
};
struct RHICmdSetSRB : RHICommand {
	ShaderResourceBinding* srb;
	RHICmdSetSRB(ShaderResourceBinding* s) : RHICommand(RHICommandType::SetSRB), srb(s) {}
};
struct RHICmdSetRenderTarget : RHICommand {
	Vector<Texture*> rtvs;
	Texture* dsv;
	Vector<ClearValue> clear_values;
	Bool has_dsv_clear;
	RHICmdSetRenderTarget(CONST Vector<Texture*>& r, Texture* d, CONST Vector<ClearValue>& c, Bool h) : RHICommand(RHICommandType::SetRenderTarget), rtvs(r), dsv(d), clear_values(c), has_dsv_clear(h) {}
};
struct RHICmdTransitionTexture : RHICommand {
	Texture* texture;
	ENUM_RESOURCE_STATE state;
	RHICmdTransitionTexture(Texture* t, CONST ENUM_RESOURCE_STATE& s) : RHICommand(RHICommandType::TransitionTexture), texture(t), state(s) {}
};
struct RHICmdClearTexture : RHICommand {
	Texture* texture;
	Vector<float> clear_value;
	RHICmdClearTexture(Texture* t, Vector<float>&& cv) : RHICommand(RHICommandType::ClearTexture), texture(t), clear_value(std::move(cv)) {}
};
struct RHICmdResourceBarrier : RHICommand {
	ENUM_RESOURCE_STATE src, dst;
	RHICmdResourceBarrier(ENUM_RESOURCE_STATE s, ENUM_RESOURCE_STATE d) : RHICommand(RHICommandType::ResourceBarrier), src(s), dst(d) {}
};
struct RHICmdFlushBarriers : RHICommand {
	// Cached image memory barrier data (platform-agnostic)
	struct CachedImageBarrier {
		UInt64 image;       // VkImage handle
		Int    old_layout;
		Int    new_layout;
		UInt32 src_access;
		UInt32 dst_access;
		UInt32 aspect_mask;
		UInt32 base_mip;
		UInt32 level_count;
		UInt32 base_layer;
		UInt32 layer_count;
	};
	Vector<CachedImageBarrier> image_barriers;
	UInt32 src_stages = 0;
	UInt32 dst_stages = 0;
	Bool   has_mem_barrier = false;
	UInt32 mem_src_access = 0;
	UInt32 mem_dst_access = 0;

	RHICmdFlushBarriers() : RHICommand(RHICommandType::FlushBarriers) {}
};
struct RHICmdSetPushConstants : RHICommand {
	UInt32 offset, size;
	ENUM_SHADER_STAGE stage = ENUM_SHADER_STAGE::Invalid;
	Vector<UInt8> data;
	RHICmdSetPushConstants(UInt32 o, UInt32 s, const void* d, ENUM_SHADER_STAGE st = ENUM_SHADER_STAGE::Invalid) : RHICommand(RHICommandType::SetPushConstants), offset(o), size(s), stage(st), data((UInt8*)d, (UInt8*)d + s) {}
};
struct RHICmdCopyBuffer : RHICommand {
	UInt64 src_id, dst_id;
	UInt32 region_count;
	Vector<UInt8> regions;
	RHICmdCopyBuffer(UInt64 s, UInt64 d, UInt32 c, const void* r, size_t sz) : RHICommand(RHICommandType::CopyBuffer), src_id(s), dst_id(d), region_count(c), regions((UInt8*)r, (UInt8*)r + sz) {}
};
struct RHICmdCopyBufferToImage : RHICommand {
	UInt64 src_buffer;   // VkBuffer handle
	UInt64 dst_image;    // VkImage handle
	Int    image_layout; // VkImageLayout
	UInt32 region_count;
	Vector<UInt8> regions; // VkBufferImageCopy[] raw data
	RHICmdCopyBufferToImage(UInt64 s, UInt64 d, Int l, UInt32 c, const void* r, size_t sz)
		: RHICommand(RHICommandType::CopyBufferToImage), src_buffer(s), dst_image(d)
		, image_layout(l), region_count(c), regions((UInt8*)r, (UInt8*)r + sz) {}
};

struct RHICmdBegin : RHICommand {
	RHICmdBegin() : RHICommand(RHICommandType::Begin) {}
};
struct RHICmdEnd : RHICommand {
	RHICmdEnd() : RHICommand(RHICommandType::End) {}
};
struct RHICmdBeginRenderPass : RHICommand {
	UInt64 render_pass;    // VkRenderPass handle
	UInt64 frame_buffer;   // VkFramebuffer handle
	UInt32 width;
	UInt32 height;
	UInt32 clear_count;
	// Clear values stored as raw data (VkClearValue is 16 bytes with union)
	Vector<UInt8> clear_value_data;
	RHICmdBeginRenderPass(UInt64 rp, UInt64 fb, UInt32 w, UInt32 h, UInt32 cc, const void* cv)
		: RHICommand(RHICommandType::BeginRenderPass), render_pass(rp), frame_buffer(fb)
		, width(w), height(h), clear_count(cc)
	{
		if (cc > 0 && cv)
			clear_value_data.assign((UInt8*)cv, (UInt8*)cv + cc * 16); // sizeof(VkClearValue) == 16
	}
};
struct RHICmdBeginUI : RHICommand { RHICmdBeginUI() : RHICommand(RHICommandType::BeginUI) {} };
struct RHICmdEndUI : RHICommand { RHICmdEndUI() : RHICommand(RHICommandType::EndUI) {} };
struct RHICmdSetScissorEnable : RHICommand {
	Bool enable;
	RHICmdSetScissorEnable(Bool e) : RHICommand(RHICommandType::SetScissorEnable), enable(e) {}
};
struct RHICmdSetScissor : RHICommand {
	Int x, y; UInt32 w, h;
	RHICmdSetScissor(Int x_, Int y_, UInt32 w_, UInt32 h_) : RHICommand(RHICommandType::SetScissor), x(x_), y(y_), w(w_), h(h_) {}
};
struct RHICmdUpdateBuffer : RHICommand {
	class Buffer* buffer;
	UInt32 offset, size;
	Vector<UInt8> data;
	RHICmdUpdateBuffer(Buffer* b, UInt32 off, UInt32 sz, const void* d)
		: RHICommand(RHICommandType::UpdateBuffer), buffer(b), offset(off), size(sz)
		, data((UInt8*)d, (UInt8*)d + sz) {}
};
struct RHICmdWriteTimestamp : RHICommand {
	UInt32 index;
	RHICmdWriteTimestamp(UInt32 i) : RHICommand(RHICommandType::WriteTimestamp), index(i) {}
};
struct RHICmdUnmapBuffer : RHICommand {
	class Buffer* buffer;
	RHICmdUnmapBuffer(class Buffer* b) : RHICommand(RHICommandType::UnmapBuffer), buffer(b) {}
};
//  ImGui draw data: recorded on Render thread, replayed on RHI thread.
//  The Editor runs in Single-thread mode (MX_THREAD_MODE=single, see
//  Editor.cpp) where the command queue bypasses and this replays inline —
//  the draw data is always the current frame's. No copy needed.
struct RHICmdRenderImGui : RHICommand {
	void* draw_data;     // ImDrawData* (valid during replay)
	void* imgui_context; // ImGuiContext* (restore before calling ImGui functions)
	RHICmdRenderImGui(void* dd, void* ctx) : RHICommand(RHICommandType::RenderImGui), draw_data(dd), imgui_context(ctx) {}
};

struct RHICmdSetVertexBuffer : RHICommand {
	class Buffer* buffer; UInt32 slot; UInt32 stride; UInt32 offset;
	RHICmdSetVertexBuffer(class Buffer* b, UInt32 sl, UInt32 st, UInt32 off)
		: RHICommand(RHICommandType::SetVertexBuffer), buffer(b), slot(sl), stride(st), offset(off) {}
};
struct RHICmdSetIndexBuffer : RHICommand {
	class Buffer* buffer; UInt32 offset; Bool index32;
	RHICmdSetIndexBuffer(class Buffer* b, UInt32 off, Bool i32)
		: RHICommand(RHICommandType::SetIndexBuffer), buffer(b), offset(off), index32(i32) {}
};
struct RHICmdDrawIndexed : RHICommand {
	UInt32 indexCount, instanceCount, firstIndex, vertexOffset, firstInstance;
	RHICmdDrawIndexed(UInt32 ic, UInt32 in, UInt32 fi, UInt32 vo, UInt32 fin)
		: RHICommand(RHICommandType::DrawIndexed), indexCount(ic), instanceCount(in), firstIndex(fi), vertexOffset(vo), firstInstance(fin) {}
};
// Indirect commands record the Buffer* itself (same convention as RHICmdSetVertexBuffer:
// the pointer stays valid during replay thanks to the WaitFrameComplete sync window).
struct RHICmdDrawIndirect : RHICommand {
	class Buffer* buffer; UInt32 offset; UInt32 draw_count; UInt32 stride;
	RHICmdDrawIndirect(class Buffer* b, UInt32 off, UInt32 dc, UInt32 st)
		: RHICommand(RHICommandType::DrawIndirect), buffer(b), offset(off), draw_count(dc), stride(st) {}
};
struct RHICmdDrawIndexedIndirect : RHICommand {
	class Buffer* buffer; UInt32 offset; UInt32 draw_count; UInt32 stride;
	RHICmdDrawIndexedIndirect(class Buffer* b, UInt32 off, UInt32 dc, UInt32 st)
		: RHICommand(RHICommandType::DrawIndexedIndirect), buffer(b), offset(off), draw_count(dc), stride(st) {}
};
struct RHICmdDispatchIndirect : RHICommand {
	class Buffer* buffer; UInt32 offset;
	RHICmdDispatchIndirect(class Buffer* b, UInt32 off)
		: RHICommand(RHICommandType::DispatchIndirect), buffer(b), offset(off) {}
};

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(CommandList,public RenderResource)

#pragma region METHOD
public:
	CommandList() MYDEFAULT;
	VIRTUAL ~CommandList() MYDEFAULT;
	VIRTUAL void METHOD(SetGraphicsPipeline)(RenderPipelineState* pipeline_state) PURE;
	VIRTUAL void METHOD(SetComputePipeline)(RenderPipelineState* pipeline_state) PURE;
	VIRTUAL void METHOD(SetRenderTarget)(CONST Vector<Texture*>& render_targets, Texture* depth_stencil, CONST Vector<ClearValue>& clear_values, Bool has_dsv_clear_value) PURE;
	VIRTUAL void METHOD(SetShaderResourceBinding)(ShaderResourceBinding* srb) PURE;
	VIRTUAL void METHOD(Draw)(CONST DrawAttribute& draw_attr) PURE;
	VIRTUAL void METHOD(SetVertexBuffer)(Buffer* buffer, UInt32 slot, UInt32 stride, UInt32 offset) {};
	VIRTUAL void METHOD(SetIndexBuffer)(Buffer* buffer, UInt32 offset, Bool index32) {};
	VIRTUAL void METHOD(DrawIndexed)(UInt32 indexCount, UInt32 instanceCount, UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance) {};
	// Indirect draws read their arguments from args_buffer (create with ENUM_BUFFER_TYPE::Indirect;
	// combine with Storage when a compute shader writes the args). If the args are GPU-written,
	// insert ResourceBarrier(UnorderedAccess, IndirectArgument) between the producing pass and the draw.
	VIRTUAL void METHOD(DrawIndirect)(Buffer* args_buffer, UInt32 args_offset, UInt32 draw_count, UInt32 stride = (UInt32)sizeof(DrawIndirectArgs)) {};
	VIRTUAL void METHOD(DrawIndexedIndirect)(Buffer* args_buffer, UInt32 args_offset, UInt32 draw_count, UInt32 stride = (UInt32)sizeof(DrawIndexedIndirectArgs)) {};
	VIRTUAL void METHOD(Dispatch)(UInt32 groupX, UInt32 groupY, UInt32 groupZ) PURE;
	VIRTUAL void METHOD(DispatchIndirect)(Buffer* args_buffer, UInt32 args_offset) {};
	VIRTUAL void METHOD(ComputeDispatch)(RenderPipelineState* pipeline, ShaderResourceBinding* srb, UInt32 groupX, UInt32 groupY, UInt32 groupZ) {}
	VIRTUAL void METHOD(SetPushConstants)(UInt32 offset, UInt32 size, const void* data) PURE;
	VIRTUAL void METHOD(SetPushConstants)(UInt32 offset, UInt32 size, const void* data, ENUM_SHADER_STAGE stage) {}
	VIRTUAL void METHOD(TransitionTextureState)(Texture* texture, CONST ENUM_RESOURCE_STATE& required_state) PURE;
	VIRTUAL void METHOD(ClearTexture)(Texture* texture,Vector<float> clear_value= Vector<float>(4,0.0f)) PURE;

	VIRTUAL void METHOD(ResourceBarrier)(ENUM_RESOURCE_STATE src_state, ENUM_RESOURCE_STATE dst_state) {}
	VIRTUAL void METHOD(MemoryBarrier)(ENUM_SHADER_STAGE src_stage, ENUM_SHADER_STAGE dst_stage, ENUM_RESOURCE_STATE src_access, ENUM_RESOURCE_STATE dst_access) {}

	VIRTUAL void METHOD(Begin)() PURE;
	VIRTUAL void METHOD(End)() PURE;

	VIRTUAL Bool METHOD(WaitForFence)(float time_in_seconds_to_wait) PURE;

	VIRTUAL void METHOD(BeginUI)() PURE;
	VIRTUAL void METHOD(EndUI)() PURE;

	// Dynamic scissor state (used by UI renderers for per-draw clipping)
	VIRTUAL void METHOD(SetScissorEnable)(bool enable) PURE;
	VIRTUAL void METHOD(SetScissor)(Int x, Int y, UInt32 w, UInt32 h) PURE;

	//  三线程模式：拆分 UI 阶段
	// Logic 线程调用（GLFW input → ImGui NewFrame）
	VIRTUAL void METHOD(BeginUI_Logic)() {}
	// Render 线程调用（ImGui widget draw → ImGui::Render → 录制 GPU 命令）
	VIRTUAL void METHOD(EndUI_Render)() {}
	// Logic 线程调用（ImGui Platform 窗口更新，需要 GLFW）
	VIRTUAL void METHOD(EndUI_Platform)() {}

	VIRTUAL void METHOD(WriteTimestamp)(UInt32 query_index) {}

	// ---- RHI Thread support ----
	void METHOD(SetBypass)(Bool b) { bypass = b; }
	Bool METHOD(IsBypass)() CONST { return bypass; }
	Vector<std::unique_ptr<RHICommand>>& METHOD(GetRecordedCommands)() { return recorded_commands; }
	VIRTUAL void METHOD(Replay)(){}

protected:
	Vector<std::unique_ptr<RHICommand>> recorded_commands;
	Bool bypass = true;

private:

#pragma endregion

#pragma region MEMBER
public:

private:

protected:
#pragma endregion

MYRENDERER_END_CLASS



MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif
