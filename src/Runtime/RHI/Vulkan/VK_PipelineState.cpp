#include "VK_PipelineState.h"
#include <cstdio>
#include "Core/ConstGlobals.h"
#include "VK_Utils.h"
#include "VK_Shader.h"
#include "RHI/RenderTexture.h"
#include "VK_RenderPass.h"
#include "Core/TypeHash.h"
#include "VK_Shader.h"
#include "VK_DescriptorSets.h"
#include "VK_BindlessManager.h"
MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

VK_PipelineState::~VK_PipelineState()
{
	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device->GetDevice(), pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}
	if (pipeline_layout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device->GetDevice(), pipeline_layout, nullptr);
		pipeline_layout = VK_NULL_HANDLE;
	}
	for (auto& it : descriptorset_layouts)
	{
		if (it != VK_NULL_HANDLE)
		{
			// Do NOT destroy the shared global bindless layout (owned by VK_BindlessManager)
			VK_BindlessManager* bindless = device->GetBindlessManager();
			if (bindless && bindless->IsEnabled() && it == bindless->GetLayout())
			{
				it = VK_NULL_HANDLE;  // Just clear the reference, don't destroy
			}
			else
			{
				vkDestroyDescriptorSetLayout(device->GetDevice(), it, nullptr);
				it = VK_NULL_HANDLE;
			}
		}
	}
}

VkPipeline VK_PipelineState::GetPipeline() CONST
{
	return pipeline;
}

VkPipelineLayout VK_PipelineState::GetPipelineLayout() CONST
{
	return pipeline_layout;
}

VK_PipelineState::VK_PipelineState(VK_Device* in_device, CONST RenderGraphiPipelineStateDesc& in_desc, VkPipelineCache pipeline_cache, CONST VK_RenderPass* render_pass): RenderPipelineState(in_desc),device(in_device)
{
	// Detect compute-only pipeline: has compute shader, no vertex/pixel shaders
	Bool is_compute = (desc.shaders[ENUM_SHADER_STAGE::Shader_Compute] != nullptr
					&& desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex] == nullptr
					&& desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel] == nullptr);
	is_compute_pipeline = is_compute;

	Vector<VkPipelineShaderStageCreateInfo> shader_stages;
	for (auto& shader : desc.shaders)
	{
		if (shader)
		{
			VK_Shader* vk_shader = static_cast<VK_Shader*>(shader);
			VkPipelineShaderStageCreateInfo stage_info = {};
			stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stage_info.stage = VK_Utils::Translate_ShaderTypeEnum_To_Vulkan(shader->GetDesc().shader_type);
			stage_info.module = vk_shader->GetShaderModule();
			stage_info.pName = shader->GetDesc().entry_name.c_str();
			shader_stages.push_back(stage_info);
		}
	}

	if (is_compute)
	{
		// === Compute Pipeline Path ===
		VkComputePipelineCreateInfo pipeline_info{};
		pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipeline_info.stage = shader_stages[0];  // Compute shader is the only stage
		pipeline_info.layout = CreatePipelineLayout(desc);
		pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
		pipeline_info.pNext = nullptr;
		CHECK_WITH_LOG( vkCreateComputePipelines(device->GetDevice(), pipeline_cache, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS,"RHI Error: Failed to create compute pipeline");
	}
	else
	{
		// === Graphics Pipeline Path (existing) ===
		VkPipelineVertexInputStateCreateInfo vertex_input_state = {};
		vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		Vector<VkVertexInputAttributeDescription> attribute_desc;
		Vector<VkVertexInputBindingDescription> binding_desc;
		auto sort_input_layout = desc.GetSortedVertexInputLayout();
		Map<UInt32,UInt32> binding_map;
		Map<UInt32, UInt8> input_rate_map;
		for(auto& input_layout : sort_input_layout)
		{
			VkVertexInputAttributeDescription single_desc = {};
			single_desc.binding = input_layout.binding;
			single_desc.location = input_layout.location;
			auto& format_attribs = Texture::GetTextureFormatAttribs(input_layout.attribute_format);
			single_desc.format = VK_Utils::Translate_Texture_Format_To_Vulkan(input_layout.attribute_format);
			single_desc.offset = input_layout.offset;
			attribute_desc.push_back(single_desc);
			binding_map[input_layout.binding] += (format_attribs.component_count* format_attribs.single_component_byte_size);
			input_rate_map[input_layout.binding] = (UInt8)input_layout.input_rate;
		}
		for (auto& binding : binding_map)
		{
			VkVertexInputBindingDescription single_binding = {};
			single_binding.binding = binding.first;
			single_binding.stride = binding.second;
			single_binding.inputRate = VK_Utils::Translation_VertexInputRate_To_Vulkan( (ENUM_VERTEX_INPUTRATE)input_rate_map[binding.first]);
			binding_desc.push_back(single_binding);
		}
		vertex_input_state.vertexAttributeDescriptionCount = attribute_desc.size();
		vertex_input_state.pVertexAttributeDescriptions = attribute_desc.data();
		vertex_input_state.vertexBindingDescriptionCount = binding_desc.size();
		vertex_input_state.pVertexBindingDescriptions = binding_desc.data();

		VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {};
		input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_state.topology = VK_Utils::Translate_PrimitiveTopology_To_Vulkan(desc.primitive_topology);
		input_assembly_state.primitiveRestartEnable = VK_FALSE;

		VkPipelineRasterizationStateCreateInfo rasterization_state = {};
		rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_state.polygonMode = VK_Utils::Translate_FillMode_To_Vulkan(desc.raster_state.fill_mode);
		rasterization_state.cullMode = VK_Utils::Translate_CullMode_To_Vulkan(desc.raster_state.cull_mode);
		rasterization_state.frontFace = desc.raster_state.front_counter_clockwise? VK_FRONT_FACE_COUNTER_CLOCKWISE: VK_FRONT_FACE_CLOCKWISE;
		rasterization_state.depthBiasEnable = desc.raster_state.depth_bias? VK_TRUE: VK_FALSE;
		rasterization_state.depthBiasConstantFactor = desc.raster_state.depth_bias;
		rasterization_state.depthBiasClamp = desc.raster_state.depth_bias_clamp;
		rasterization_state.depthBiasSlopeFactor = desc.raster_state.depth_bias_slope_scaled;
		rasterization_state.lineWidth = 1.0f;

		Vector< VkPipelineColorBlendAttachmentState> color_blend_attachment_state ;
		color_blend_attachment_state.resize(desc.blend_state.render_targets.size());
		for(UInt32 i = 0; i < (UInt32)color_blend_attachment_state.size(); i++)
		{
			// ENUM_COLOR_MASK bits match VkColorComponentFlagBits (R=1,G=2,B=4,A=8) one-to-one.
			color_blend_attachment_state[i].colorWriteMask = (VkColorComponentFlags)desc.blend_state.render_targets[i].write_mask;
			color_blend_attachment_state[i].blendEnable = desc.blend_state.render_targets[i].blend_enable? VK_TRUE: VK_FALSE;
			color_blend_attachment_state[i].srcColorBlendFactor = VK_Utils::Translate_BlendFactor_To_Vulkan(desc.blend_state.render_targets[i].src_color);
			color_blend_attachment_state[i].dstColorBlendFactor = VK_Utils::Translate_BlendFactor_To_Vulkan(desc.blend_state.render_targets[i].dst_color);
			color_blend_attachment_state[i].colorBlendOp = VK_Utils::Translate_BlendOp_To_Vulkan(desc.blend_state.render_targets[i].op_color);
			color_blend_attachment_state[i].srcAlphaBlendFactor = VK_Utils::Translate_BlendFactor_To_Vulkan(desc.blend_state.render_targets[i].src_alpha);
			color_blend_attachment_state[i].dstAlphaBlendFactor = VK_Utils::Translate_BlendFactor_To_Vulkan(desc.blend_state.render_targets[i].dst_alpha);
			color_blend_attachment_state[i].alphaBlendOp = VK_Utils::Translate_BlendOp_To_Vulkan(desc.blend_state.render_targets[i].op_alpha);
		}

		VkPipelineColorBlendStateCreateInfo color_blend_state = {};
		color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_state.attachmentCount = color_blend_attachment_state.size();
		color_blend_state.pAttachments = color_blend_attachment_state.data();

		VkPipelineDepthStencilStateCreateInfo depth_stencil_state = {};
		depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_state.depthTestEnable = desc.depth_stencil_state.depth_test_enable;
		depth_stencil_state.depthWriteEnable = desc.depth_stencil_state.depth_write_enable;
		depth_stencil_state.depthCompareOp = VK_Utils::Translate_CompareOp_To_Vulkan(desc.depth_stencil_state.depth_func);
		depth_stencil_state.stencilTestEnable = desc.depth_stencil_state.stencil_test_enable;
		VkStencilOpState front = VK_Utils::Translate_StencilOpState_To_Vulkan(desc.depth_stencil_state.front_face_stencil);
		front.compareMask = desc.depth_stencil_state.stencil_read_mask;
		front.writeMask = desc.depth_stencil_state.stencil_write_mask;
		front.reference = desc.depth_stencil_state.stencil_ref;
		depth_stencil_state.front = front;
		VkStencilOpState back = VK_Utils::Translate_StencilOpState_To_Vulkan(desc.depth_stencil_state.back_face_stencil);
		back.compareMask = desc.depth_stencil_state.stencil_read_mask;
		back.writeMask = desc.depth_stencil_state.stencil_write_mask;
		back.reference = desc.depth_stencil_state.stencil_ref;
		depth_stencil_state.back = back;
		depth_stencil_state.depthBoundsTestEnable= desc.depth_stencil_state.depth_bounds_test_enable;
		depth_stencil_state.minDepthBounds = desc.depth_stencil_state.min_depth_bounds;
		depth_stencil_state.maxDepthBounds = desc.depth_stencil_state.max_depth_bounds;

		VkPipelineViewportStateCreateInfo viewport_state = {};
		viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_state.viewportCount = 1;
		viewport_state.scissorCount = 1;

		VkPipelineMultisampleStateCreateInfo multisample_state = {};
		multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_state.rasterizationSamples = VK_Utils::Get_SampleCountFlagBits_FromInt(desc.raster_state.sample_count) ;
		multisample_state.alphaToCoverageEnable = desc.blend_state.enable_alpha_to_coverage;

		Vector<VkDynamicState> dynamic_state_enables {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamic_state = {};
		dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_state.dynamicStateCount = 2;
		dynamic_state.pDynamicStates = dynamic_state_enables.data();

		// Tessellation state: required when the topology is PatchList (hull/domain shaders)
		VkPipelineTessellationStateCreateInfo tessellation_state{};
		tessellation_state.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
		tessellation_state.patchControlPoints = desc.patch_control_points ? desc.patch_control_points : 3;

		VkGraphicsPipelineCreateInfo pipeline_info{};
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.stageCount = shader_stages.size();
		pipeline_info.pStages = shader_stages.data();
		pipeline_info.pVertexInputState = &vertex_input_state;
		pipeline_info.pInputAssemblyState = &input_assembly_state;
		pipeline_info.pViewportState = &viewport_state;
		pipeline_info.pRasterizationState = &rasterization_state;
		pipeline_info.pMultisampleState = &multisample_state;
		pipeline_info.pDepthStencilState = &depth_stencil_state;
		pipeline_info.pColorBlendState = &color_blend_state;
		pipeline_info.pDynamicState = &dynamic_state;
		if (desc.primitive_topology == ENUM_PRIMITIVE_TYPE::PatchList)
			pipeline_info.pTessellationState = &tessellation_state;
		pipeline_info.layout = CreatePipelineLayout(desc);
		VkPipelineRenderingCreateInfoKHR rendering_info{};
		Vector<VkFormat> dyn_color_formats;
		if (render_pass == nullptr)
		{
			rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
			rendering_info.colorAttachmentCount = (UInt32)desc.render_targets.size();
			for (auto& rtv : desc.render_targets)
				dyn_color_formats.push_back(VK_Utils::Translate_Texture_Format_To_Vulkan(rtv->GetTextureDesc().format));
			rendering_info.pColorAttachmentFormats = dyn_color_formats.data();
			if (desc.depth_stencil_view)
				rendering_info.depthAttachmentFormat = VK_Utils::Translate_Texture_Format_To_Vulkan(desc.depth_stencil_view->GetTextureDesc().format);
			pipeline_info.pNext = &rendering_info;
			pipeline_info.renderPass = VK_NULL_HANDLE;
		}
		else
		{
			pipeline_info.renderPass = render_pass->GetRenderPass();
		}
		pipeline_info.subpass = 0;
		pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
		CHECK_WITH_LOG( vkCreateGraphicsPipelines(device->GetDevice(), pipeline_cache, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS,"RHI Error: Failed to create pipeline");
	}
}


VkPipelineLayout VK_PipelineState::CreatePipelineLayout(CONST RenderGraphiPipelineStateDesc& in_desc)
{
	if(pipeline_layout != VK_NULL_HANDLE)
	{
		return pipeline_layout;
	}

	Array<DescriptorSetLayoutData, MYRENDER_MAX_BINDING_SET_NUM> merged_layouts;

	for (UInt32 i = 0; i < MYRENDER_MAX_BINDING_SET_NUM; i++)
	{

		DescriptorSetLayoutData& ly = merged_layouts[i];

		ly.set_number = i;

		ly.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

		Map<UInt32, VkDescriptorSetLayoutBinding> binds;
		for (UInt32 shader_index =0; shader_index < ENUM_SHADER_STAGE::NumStages; ++shader_index)
		{
			if (desc.shaders[shader_index])
			{
				auto& reflect_info = ((VK_Shader*)desc.shaders[shader_index])->GetReflectedInfo();
				for (auto& s : reflect_info.setlayouts) 
				{
					if (s.set_number == i) 
					{
						for (auto& b : s.bindings)
						{
							auto it = binds.find(b.binding);
							if (it == binds.end())
							{
								binds[b.binding] = b;
								//ly.bindings.push_back(b);
							}
							else 
							{
								//merge flags
								binds[b.binding].stageFlags |= b.stageFlags;
							}

						}
					}
				}
			}
		}
		for (auto [k, v] : binds)
		{
			ly.bindings.push_back(v);
		}
		//sort the bindings, for hash purposes
		std::sort(ly.bindings.begin(), ly.bindings.end(), [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b) {
			return a.binding < b.binding;
		});


		// Detect if this set uses the global bindless heap (Set 3 with bindings)
		ly.is_bindless = (i == VK_BindlessManager::BINDLESS_SET_INDEX && ly.bindings.size() > 0);

		ly.create_info.bindingCount = (uint32_t)ly.bindings.size();
		ly.create_info.pBindings = ly.bindings.data();
		ly.create_info.flags = 0;
		ly.create_info.pNext = 0;


		if (ly.is_bindless && device->GetBindlessManager() && device->GetBindlessManager()->IsEnabled())
		{
			// Use the shared global bindless descriptor set layout (all pipelines with Set 3 share this)
			descriptorset_layouts[i] = device->GetBindlessManager()->GetLayout();
		}
		else if (ly.create_info.bindingCount > 0)
		{
			CHECK_WITH_LOG( vkCreateDescriptorSetLayout(device->GetDevice(), &ly.create_info, nullptr, &descriptorset_layouts[i])!=VK_SUCCESS, "RHI Error: Failed to create descriptor set layout");
		}
		else
		{
			descriptorset_layouts[i] = VK_NULL_HANDLE;
		}
	}

	//merge constantbuffers
	Vector< VkPushConstantRange> constant_ranges;
	Map<String, VkPushConstantRange> constant_ranges_map;
	for (UInt32 shader_index=0; shader_index < ENUM_SHADER_STAGE::NumStages; ++shader_index)
	{
		if (desc.shaders[shader_index])
		{
			auto& reflect_info = ((VK_Shader*)desc.shaders[shader_index])->GetReflectedInfo();
			for (auto& s : reflect_info.constant_ranges) 
			{
				constant_ranges_map[s.name] = s.constant;
			}
		}
	}
	for (auto [k, v] : constant_ranges_map)
	{
		constant_ranges.push_back(v);
	}


	//we start from just the default empty pipeline layout info
	VkPipelineLayoutCreateInfo pipeline_layout_info = VK_Utils::Pipeline_Layout_Create_Info();

	pipeline_layout_info.pPushConstantRanges = constant_ranges.data();
	pipeline_layout_info.pushConstantRangeCount = (uint32_t)constant_ranges.size();

	Array<VkDescriptorSetLayout, MYRENDER_MAX_BINDING_SET_NUM> compacted_layouts;
	int s = 0;
	for (int i = 0; i < MYRENDER_MAX_BINDING_SET_NUM; i++)
	{
		if (descriptorset_layouts[i] != VK_NULL_HANDLE)
		{
			compacted_layouts[s] = descriptorset_layouts[i];
			s++;
		}
	}

	pipeline_layout_info.setLayoutCount = s;
	pipeline_layout_info.pSetLayouts = compacted_layouts.data();

	CHECK_WITH_LOG( vkCreatePipelineLayout(device->GetDevice(), &pipeline_layout_info, nullptr, &pipeline_layout)!=VK_SUCCESS, "RHI Error: Failed to create pipeline layout");
	
	for (UInt32 shader_index = 0; shader_index < ENUM_SHADER_STAGE::NumStages; ++shader_index)
	{
		if (desc.shaders[shader_index])
		{
			auto& reflect_info = ((VK_Shader*)desc.shaders[shader_index])->GetReflectedInfo();
			for (auto& s : reflect_info.bindings)
			{
				compacted_bindings[s.first] = s.second;
			}
		}
	}
	
	return pipeline_layout;
}

void VK_PipelineState::CreateShaderResourceBinding(ShaderResourceBinding*& out_srb, Bool init_static_resource /*= false*/)
{
	if (out_srb == nullptr)
		out_srb = new VK_ShaderResourceBinding(device,compacted_bindings, init_static_resource);
	VK_ShaderResourceBinding* vk_srb = STATIC_CAST(out_srb, VK_ShaderResourceBinding);
	VK_DescriptsetAllocator* allocator = device->GetDescriptsetAllocator();
	for (UInt32 i = 0; i < 4; i++)
	{
		if (descriptorset_layouts[i] != VK_NULL_HANDLE)
		{
			// Check if this set uses the global bindless descriptor set
			if (device->GetBindlessManager() && device->GetBindlessManager()->IsEnabled()
				&& descriptorset_layouts[i] == device->GetBindlessManager()->GetLayout())
			{
				vk_srb->descriptorset[i] = device->GetBindlessManager()->GetDescriptorSet();
			}
			else
			{
				allocator->AllocateDescriptorset(descriptorset_layouts[i], vk_srb->descriptorset[i]);
			}
		}
		else
		{
			vk_srb->descriptorset[i] = VK_NULL_HANDLE;
		}
	}
}

// --   VK_ComputePipelineState implementation

VK_ComputePipelineState::~VK_ComputePipelineState()
{
        if (pipeline != VK_NULL_HANDLE)
        {
                vkDestroyPipeline(device->GetDevice(), pipeline, nullptr);
                pipeline = VK_NULL_HANDLE;
        }
        if (pipeline_layout != VK_NULL_HANDLE)
        {
                vkDestroyPipelineLayout(device->GetDevice(), pipeline_layout, nullptr);
                pipeline_layout = VK_NULL_HANDLE;
        }
        for (auto& it : descriptorset_layouts)
        {
                if (it != VK_NULL_HANDLE)
                {
                        VK_BindlessManager* bindless = device->GetBindlessManager();
                        if (bindless && bindless->IsEnabled() && it == bindless->GetLayout())
                        {
                                it = VK_NULL_HANDLE;
                        }
                        else
                        {
                                vkDestroyDescriptorSetLayout(device->GetDevice(), it, nullptr);
                                it = VK_NULL_HANDLE;
                        }
                }
        }
}

VkPipeline VK_ComputePipelineState::GetPipeline() CONST
{
        return pipeline;
}

VkPipelineLayout VK_ComputePipelineState::GetPipelineLayout() CONST
{
        return pipeline_layout;
}

VK_ComputePipelineState::VK_ComputePipelineState(VK_Device* in_device, CONST ComputePipelineStateDesc& in_desc, VkPipelineCache pipeline_cache)
        : ComputePipelineState(in_desc), device(in_device)
{
        VK_Shader* vk_shader = static_cast<VK_Shader*>(desc.compute_shader);

        VkPipelineShaderStageCreateInfo stage_info = {};
        stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage = VK_Utils::Translate_ShaderTypeEnum_To_Vulkan(vk_shader->GetDesc().shader_type);
        stage_info.module = vk_shader->GetShaderModule();
        stage_info.pName = vk_shader->GetDesc().entry_name.c_str();

        VkComputePipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipeline_info.stage = stage_info;
        pipeline_info.layout = CreatePipelineLayout();
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
        pipeline_info.pNext = nullptr;
        CHECK_WITH_LOG(vkCreateComputePipelines(device->GetDevice(), pipeline_cache, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS,
                "RHI Error: Failed to create compute pipeline");
}

VkPipelineLayout VK_ComputePipelineState::CreatePipelineLayout()
{
        if (pipeline_layout != VK_NULL_HANDLE)
                return pipeline_layout;

        Array<DescriptorSetLayoutData, MYRENDER_MAX_BINDING_SET_NUM> merged_layouts;
        VK_Shader* vk_shader = static_cast<VK_Shader*>(desc.compute_shader);
        auto& reflect_info = vk_shader->GetReflectedInfo();

        for (UInt32 i = 0; i < MYRENDER_MAX_BINDING_SET_NUM; i++)
        {
                DescriptorSetLayoutData& ly = merged_layouts[i];
                ly.set_number = i;
                ly.create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

                for (auto& s : reflect_info.setlayouts)
                {
                        if (s.set_number == i)
                        {
                                ly.bindings = s.bindings;
                                break;
                        }
                }

                std::sort(ly.bindings.begin(), ly.bindings.end(), [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b) {
                        return a.binding < b.binding;
                });

                ly.is_bindless = (i == VK_BindlessManager::BINDLESS_SET_INDEX && ly.bindings.size() > 0);
                ly.create_info.bindingCount = (uint32_t)ly.bindings.size();
                ly.create_info.pBindings = ly.bindings.data();
                ly.create_info.flags = 0;
                ly.create_info.pNext = 0;

                if (ly.is_bindless && device->GetBindlessManager() && device->GetBindlessManager()->IsEnabled())
                {
                        descriptorset_layouts[i] = device->GetBindlessManager()->GetLayout();
                }
                else if (ly.create_info.bindingCount > 0)
                {
                        CHECK_WITH_LOG(vkCreateDescriptorSetLayout(device->GetDevice(), &ly.create_info, nullptr, &descriptorset_layouts[i]) != VK_SUCCESS,
                                "RHI Error: Failed to create compute descriptor set layout");
                }
                else
                {
                        descriptorset_layouts[i] = VK_NULL_HANDLE;
                }
        }

        Vector<VkPushConstantRange> constant_ranges;
        for (auto& s : reflect_info.constant_ranges)
                constant_ranges.push_back(s.constant);

        VkPipelineLayoutCreateInfo pipeline_layout_info = VK_Utils::Pipeline_Layout_Create_Info();
        pipeline_layout_info.pPushConstantRanges = constant_ranges.data();
        pipeline_layout_info.pushConstantRangeCount = (uint32_t)constant_ranges.size();

        Array<VkDescriptorSetLayout, MYRENDER_MAX_BINDING_SET_NUM> compacted_layouts;
        UInt32 compact_count = 0;
        for (UInt32 i = 0; i < MYRENDER_MAX_BINDING_SET_NUM; i++)
        {
                if (descriptorset_layouts[i] != VK_NULL_HANDLE)
                        compacted_layouts[compact_count++] = descriptorset_layouts[i];
        }
        pipeline_layout_info.setLayoutCount = compact_count;
        pipeline_layout_info.pSetLayouts = compacted_layouts.data();

        CHECK_WITH_LOG(vkCreatePipelineLayout(device->GetDevice(), &pipeline_layout_info, nullptr, &pipeline_layout) != VK_SUCCESS,
                "RHI Error: Failed to create compute pipeline layout");

        for (auto& [name, binding] : reflect_info.bindings)
                compacted_bindings[name] = binding;

        return pipeline_layout;
}

void VK_ComputePipelineState::CreateShaderResourceBinding(ShaderResourceBinding*& out_srb, Bool init_static_resource)
{
        if (out_srb == nullptr)
                out_srb = new VK_ShaderResourceBinding(device, compacted_bindings, init_static_resource);
        VK_ShaderResourceBinding* vk_srb = STATIC_CAST(out_srb, VK_ShaderResourceBinding);
        VK_DescriptsetAllocator* allocator = device->GetDescriptsetAllocator();
        for (UInt32 i = 0; i < 4; i++)
        {
                if (descriptorset_layouts[i] != VK_NULL_HANDLE)
                {
                        if (device->GetBindlessManager() && device->GetBindlessManager()->IsEnabled()
                                && descriptorset_layouts[i] == device->GetBindlessManager()->GetLayout())
                        {
                                vk_srb->descriptorset[i] = device->GetBindlessManager()->GetDescriptorSet();
                        }
                        else
                        {
                                allocator->AllocateDescriptorset(descriptorset_layouts[i], vk_srb->descriptorset[i]);
                        }
                }
                else
                {
                        vk_srb->descriptorset[i] = VK_NULL_HANDLE;
                }
        }
}
// --  

VK_PipelineStateManager::VK_PipelineStateManager(VK_Device* in_device):device(in_device)
{
	FILE* f = fopen("PipelineCache.bin", "rb");
	VkPipelineCacheCreateInfo info = {}; info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	if (f) { fseek(f,0,SEEK_END); size_t sz=ftell(f); fseek(f,0,SEEK_SET);
		if(sz>0){ void* d=malloc(sz); if(fread(d,1,sz,f)==sz){ info.pInitialData=d; info.initialDataSize=sz; } free(d); } fclose(f); }
	CHECK_WITH_LOG(vkCreatePipelineCache(in_device->GetDevice(),&info,nullptr,&pipeline_cache)!=VK_SUCCESS,"RHI Error: Failed to create pipeline cache");
}

VK_PipelineStateManager::~VK_PipelineStateManager()
{
	if(pipeline_cache){ size_t sz=0; vkGetPipelineCacheData(device->GetDevice(),pipeline_cache,&sz,nullptr);
		if(sz>0){ void* d=malloc(sz); if(vkGetPipelineCacheData(device->GetDevice(),pipeline_cache,&sz,d)==VK_SUCCESS)
			{ FILE* f=fopen("PipelineCache.bin","wb"); if(f){ fwrite(d,1,sz,f); fclose(f); } } free(d); }
		vkDestroyPipelineCache(device->GetDevice(),pipeline_cache,nullptr); pipeline_cache=VK_NULL_HANDLE; }
	ProcessPendingDestruction();
	for(auto& it:pipeline_states_map) delete it.second;
	pipeline_states_map.clear();
	for(auto& it:compute_pipeline_states_map) delete it.second;
	compute_pipeline_states_map.clear();
}

VkPipelineCache VK_PipelineStateManager::GetPipelineCache() CONST
{
	return pipeline_cache;
}

VK_PipelineState* VK_PipelineStateManager::GetPipelineState(CONST RenderGraphiPipelineStateDesc& in_desc, CONST VK_RenderPass* render_pass)
{
	UInt64 hash = render_pass? HashCombine(in_desc.GetHash(), std::hash< CONST VK_RenderPass*>{}(render_pass)) : in_desc.GetHash();
	auto it = pipeline_states_map.find(hash);
	if (it != pipeline_states_map.end())
	{
		// Verify desc matches (hash=0 bug could cause false cache hits)
		if (it->second->desc == in_desc) {
			it->second->last_used_frame = g_frame_number_render_thread.load();
			std::cout << "use cache PipelineState:" << in_desc.shaders[ENUM_SHADER_STAGE::Shader_Compute]->GetDesc().shader_name << std::endl;
			return it->second;
		}

		// Hash collision or stale pointer match -- evict the old entry
		// and fall through to create a new one. The else-before was
		// causing undefined return value on hash hit with mismatched desc.
		pipeline_states_map.erase(it);
	}
	// removed else -- hash collision now handled correctly
	VK_PipelineState* pipeline_state = new VK_PipelineState(device, in_desc, pipeline_cache, render_pass);
	pipeline_states_map[hash] = pipeline_state;
	EvictLRU();
	return pipeline_state;
}

// --  
VK_ComputePipelineState* VK_PipelineStateManager::GetComputePipelineState(CONST ComputePipelineStateDesc& in_desc)
{
        UInt64 hash = std::hash<const void*>{}(in_desc.compute_shader);
        auto it = compute_pipeline_states_map.find(hash);
        if (it != compute_pipeline_states_map.end())
        {
                it->second->last_used_frame = g_frame_number_render_thread.load();
                return it->second;
        }
        VK_ComputePipelineState* pipeline_state = new VK_ComputePipelineState(device, in_desc, pipeline_cache);
        compute_pipeline_states_map[hash] = pipeline_state;
        EvictLRU();
        return pipeline_state;
}

void VK_PipelineStateManager::EvictLRU() {}
void VK_PipelineStateManager::ProcessPendingDestruction() { for (VkPipeline p : pending_destruction) { if (p) vkDestroyPipeline(device->GetDevice(), p, nullptr); } pending_destruction.clear(); }
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
