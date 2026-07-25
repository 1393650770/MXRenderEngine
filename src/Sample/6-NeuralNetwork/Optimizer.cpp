#include "Optimizer.h"
#include "Layer.h"
#include <fstream>
#include <iostream>
#include "Core/ConstDefine.h"
#include "RHI/RenderEnum.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderShader.h"
#include "RHI/RenderResource.h"
// --  
#include "ShaderHelper.h"
// --   VK_Shader.h removed - FlushDescriptorWrites now on RHI base class

using namespace MXRender::RHI;
using namespace MXRender;

namespace MXNN {

SGD::SGD(Float32 in_lr, Float32 in_momentum, Float32 in_weight_decay)
	: lr_(in_lr), momentum_(in_momentum), weight_decay_(in_weight_decay), pc_buf_({5})
{
	Shader* shader = LoadComputeShader("Shader/nn_update_sgd.comp.spv");
	update_pipeline_ = CreateComputePipeline(shader);
	update_pipeline_->CreateShaderResourceBinding(update_srb_, false);
	update_srb_->SetResource("pc", pc_buf_.GetBuffer());
	delete shader;
}

SGD::~SGD()
{
	if (update_srb_) delete update_srb_;
		for (auto* srb : temp_srbs_) delete srb;
}

void SGD::Update(CommandList* in_cmd, Tensor& in_params, Tensor& in_grads,
	Tensor& in_velocity, Float32 in_inv_batch_size, UInt32 /*in_step*/)
{
	SgdParams params;
	params.lr = lr_;
	params.momentum = momentum_;
	params.wd = weight_decay_;
	params.inv_bs = in_inv_batch_size;
	params.n_elem = static_cast<Float32>(in_params.ElementCount());
	pc_buf_.Upload(&params.lr);

	ShaderResourceBinding* temp_srb = nullptr;
	update_pipeline_->CreateShaderResourceBinding(temp_srb, false);
	temp_srb->SetResource("p0", in_params.GetBuffer());
	temp_srb->SetResource("g0", in_grads.GetBuffer());
	temp_srb->SetResource("v0", in_velocity.GetBuffer());
	temp_srb->SetResource("pc", pc_buf_.GetBuffer());
	temp_srb->FlushDescriptorWrites(); // --   virtual dispatch — no Vulkan cast needed

	in_cmd->SetComputePipeline(update_pipeline_);
	in_cmd->SetShaderResourceBinding(temp_srb);
	in_cmd->Dispatch((static_cast<UInt32>(params.n_elem) + 255u) / 256u, 1, 1);

	temp_srbs_.push_back(temp_srb);
}

// --   Adam implementation
Adam::Adam(Params in_p) : p_(in_p) {
	Shader* s = LoadComputeShader("Shader/nn_update_adam.comp.spv");
	pipeline_ = CreateComputePipeline(s);
	pipeline_->CreateShaderResourceBinding(srb_, false);
	delete s;
}
Adam::~Adam() {
	for (auto* s : temp_srbs_) delete s;
	delete srb_;
	// pipeline_ owned by VK_PipelineStateManager
	for (auto& kv : v_map_) delete kv.second;
}
void Adam::Update(CommandList* in_cmd, Tensor& in_params, Tensor& in_grads,
	Tensor& in_velocity, Float32 in_inv_batch_size, UInt32 in_step)
{
	if (v_map_.find(&in_params) == v_map_.end()) {
		v_map_[&in_params] = new Tensor(in_grads.Shape());
	}
	Tensor* v_tensor = v_map_[&in_params];
	Float32 pc[9] = { p_.lr, p_.beta1, p_.beta2, p_.eps, p_.wd, in_inv_batch_size, (Float32)in_step, (Float32)in_grads.ElementCount(), 0.0f };
	bool is_adamw = (typeid(*this) == typeid(AdamW));
	if (is_adamw) pc[8] = 1.0f;
	pc_buf_.Upload(pc);
	ShaderResourceBinding* s = nullptr;
	pipeline_->CreateShaderResourceBinding(s, false);
	s->SetResource("p", in_params.GetBuffer());
	s->SetResource("g", in_grads.GetBuffer());
	s->SetResource("m", in_velocity.GetBuffer());
	s->SetResource("v", v_tensor->GetBuffer());
	s->SetResource("pc", pc_buf_.GetBuffer());
	s->FlushDescriptorWrites();
	in_cmd->SetComputePipeline(pipeline_);
	in_cmd->SetShaderResourceBinding(s);
	in_cmd->Dispatch((in_grads.ElementCount() + 255u) / 256u, 1u, 1u);
	temp_srbs_.push_back(s);
}
// --  

} // namespace MXNN
