#if PLATFORM_WGPU

#include "RHI/WGPU/WGPU_CommandBuffer.h"
#include "RHI/WGPU/WGPU_Texture.h"
#include "RHI/WGPU/WGPU_PipelineState.h"
#include "RHI/WGPU/WGPU_ShaderResourceBinding.h"
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(WebGPU)

WGPU_CommandBuffer::WGPU_CommandBuffer(WGPUDevice device)
	: m_device(device)
{
	bypass = true;  // Single-threaded: execute immediately, no recording
}

WGPU_CommandBuffer::~WGPU_CommandBuffer()
{
	if (m_render_pass) { wgpuRenderPassEncoderRelease(m_render_pass); m_render_pass = nullptr; }
	if (m_compute_pass) { wgpuComputePassEncoderRelease(m_compute_pass); m_compute_pass = nullptr; }
	if (m_encoder) { wgpuCommandEncoderRelease(m_encoder); m_encoder = nullptr; }
	if (m_cmd_buffer) { wgpuCommandBufferRelease(m_cmd_buffer); m_cmd_buffer = nullptr; }
}

#pragma region LIFECYCLE

void WGPU_CommandBuffer::Begin()
{
	// Idempotent: if already begun (encoder exists), do nothing.
	// This matches the VK backend pattern where GetImmediateCommandList
	// calls Begin() each time it's accessed.
	if (m_encoder) return;

	// Release any stale state from previous frame
	if (m_render_pass) { wgpuRenderPassEncoderRelease(m_render_pass); m_render_pass = nullptr; }
	if (m_compute_pass) { wgpuComputePassEncoderRelease(m_compute_pass); m_compute_pass = nullptr; }
	if (m_cmd_buffer) { wgpuCommandBufferRelease(m_cmd_buffer); m_cmd_buffer = nullptr; }
	m_current_graphics_pso = nullptr;

	CHECK_WITH_LOG(m_device == nullptr, "WGPU: Begin called with null device");

	WGPUCommandEncoderDescriptor enc_desc{};
	m_encoder = wgpuDeviceCreateCommandEncoder(m_device, &enc_desc);
	CHECK_WITH_LOG(m_encoder == nullptr, "WGPU: wgpuDeviceCreateCommandEncoder failed");
}

void WGPU_CommandBuffer::End()
{
	EndActiveRenderPass();
	EndActiveComputePass();
	m_current_graphics_pso = nullptr;

	CHECK_WITH_LOG(m_encoder == nullptr, "WGPU: End called with null encoder");

	m_cmd_buffer = wgpuCommandEncoderFinish(m_encoder, nullptr);
	CHECK_WITH_LOG(m_cmd_buffer == nullptr, "WGPU: wgpuCommandEncoderFinish failed");

	wgpuCommandEncoderRelease(m_encoder);
	m_encoder = nullptr;
}

void WGPU_CommandBuffer::ConsumeEncodedCommandBuffer()
{
	if (m_cmd_buffer)
	{
		wgpuCommandBufferRelease(m_cmd_buffer);
		m_cmd_buffer = nullptr;
	}
}

#pragma endregion

#pragma region RENDER_PASS

void WGPU_CommandBuffer::EndActiveRenderPass()
{
	if (m_render_pass)
	{
		wgpuRenderPassEncoderEnd(m_render_pass);
		wgpuRenderPassEncoderRelease(m_render_pass);
		m_render_pass = nullptr;
	}
}

void WGPU_CommandBuffer::EndActiveComputePass()
{
	if (m_compute_pass)
	{
		wgpuComputePassEncoderEnd(m_compute_pass);
		wgpuComputePassEncoderRelease(m_compute_pass);
		m_compute_pass = nullptr;
	}
}

void WGPU_CommandBuffer::SetRenderTarget(CONST Vector<Texture*>& render_targets, Texture* depth_stencil, CONST Vector<ClearValue>& clear_values, Bool has_dsv_clear_value)
{
	CHECK_WITH_LOG(m_encoder == nullptr, "WGPU: SetRenderTarget called without Begin");

	// WebGPU: a render pass must be ended before starting a new one
	EndActiveRenderPass();
	EndActiveComputePass();

	// Build color attachments from render_targets
	// HelloTriangle path: single swapchain RTV, no depth
	Vector<WGPURenderPassColorAttachment> color_attachments;
	color_attachments.reserve(render_targets.size());

	for (UInt32 i = 0; i < render_targets.size(); ++i)
	{
		auto* wgpu_tex = static_cast<WGPU_Texture*>(render_targets[i]);
		CHECK_WITH_LOG(wgpu_tex == nullptr || wgpu_tex->GetView() == nullptr, "WGPU: SetRenderTarget null texture view");

		WGPURenderPassColorAttachment attachment{};
		attachment.view = wgpu_tex->GetView();
		attachment.resolveTarget = nullptr;

		// Pick clear value if available, else Load
		if (i < clear_values.size())
		{
			attachment.loadOp = WGPULoadOp_Clear;
			const Float32* c = clear_values[i].color;
			attachment.clearValue = WGPUColor{ (double)c[0], (double)c[1], (double)c[2], (double)c[3] };
		}
		else
		{
			attachment.loadOp = WGPULoadOp_Load;
			attachment.clearValue = WGPUColor{ 0.0, 0.0, 0.0, 1.0 };
		}
		attachment.storeOp = WGPUStoreOp_Store;
		color_attachments.push_back(attachment);
	}

	WGPURenderPassDescriptor rp_desc{};
	rp_desc.colorAttachments = color_attachments.data();
	rp_desc.colorAttachmentCount = (UInt32)color_attachments.size();
	rp_desc.depthStencilAttachment = nullptr;  // HelloTriangle: no depth

	m_render_pass = wgpuCommandEncoderBeginRenderPass(m_encoder, &rp_desc);
	CHECK_WITH_LOG(m_render_pass == nullptr, "WGPU: wgpuCommandEncoderBeginRenderPass failed");

	// Re-bind the current graphics pipeline on the new render pass
	if (m_current_graphics_pso && m_current_graphics_pso->GetPipeline())
	{
		wgpuRenderPassEncoderSetPipeline(m_render_pass, m_current_graphics_pso->GetPipeline());
	}
}

void WGPU_CommandBuffer::SetGraphicsPipeline(RenderPipelineState* pipeline_state)
{
	CHECK_WITH_LOG(pipeline_state == nullptr, "WGPU: SetGraphicsPipeline null pso");

	m_current_graphics_pso = static_cast<WGPU_PipelineState*>(pipeline_state);

	if (m_render_pass && m_current_graphics_pso->GetPipeline())
	{
		wgpuRenderPassEncoderSetPipeline(m_render_pass, m_current_graphics_pso->GetPipeline());
	}
	// If no render pass active yet, pipeline will be bound when SetRenderTarget starts the pass
}

void WGPU_CommandBuffer::SetComputePipeline(RenderPipelineState* pipeline_state)
{
	// Phase C stub: HelloTriangle doesn't use compute
	// Future: end render pass, begin compute pass, wgpuComputePassEncoderSetPipeline
	std::cout << "[WGPU] SetComputePipeline: stub (not used by HelloTriangle)" << std::endl;
}

void WGPU_CommandBuffer::SetShaderResourceBinding(ShaderResourceBinding* srb)
{
	// HelloTriangle: SRB is empty (no bind group). No-op.
	// Future: wgpuRenderPassEncoderSetBindGroup(m_render_pass, 0, bind_group, 0, nullptr);
	if (!m_render_pass || !srb) return;

	auto* wgpu_srb = static_cast<WGPU_ShaderResourceBinding*>(srb);
	WGPUBindGroup bind_group = wgpu_srb->GetBindGroup();
	if (bind_group)
	{
		wgpuRenderPassEncoderSetBindGroup(m_render_pass, 0, bind_group, 0, nullptr);
	}
}

#pragma endregion

#pragma region DRAW

void WGPU_CommandBuffer::Draw(CONST DrawAttribute& draw_attr)
{
	CHECK_WITH_LOG(m_render_pass == nullptr, "WGPU: Draw called without active render pass");
	wgpuRenderPassEncoderDraw(m_render_pass,
		draw_attr.vertexCount,
		draw_attr.instanceCount,
		draw_attr.firstVertex,
		draw_attr.firstInstance);
}

void WGPU_CommandBuffer::Dispatch(UInt32 groupX, UInt32 groupY, UInt32 groupZ)
{
	// Phase C stub: HelloTriangle doesn't use compute
	std::cout << "[WGPU] Dispatch: stub (not used by HelloTriangle)" << std::endl;
}

#pragma endregion

#pragma region MISC

void WGPU_CommandBuffer::SetPushConstants(UInt32 offset, UInt32 size, const void* data)
{
	// WebGPU has no push constants: use uniform buffers instead. No-op.
}

Bool WGPU_CommandBuffer::WaitForFence(float time_in_seconds_to_wait)
{
	// Single-threaded mode: all commands are synchronous, no fences needed.
	return true;
}

#pragma endregion

MYRENDERER_END_NAMESPACE  // WebGPU
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_WGPU
