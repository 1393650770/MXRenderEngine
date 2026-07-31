

#include "VK_Utils.h"






namespace MXRender
{



	VkAccessFlags VK_Utils::AccessMaskFromImageLayout(VkImageLayout Layout, bool IsDstMask)
	{
		VkAccessFlags AccessMask = 0;
		switch (Layout)
		{
			// does not support device access. This layout must only be used as the initialLayout member
			// of VkImageCreateInfo or VkAttachmentDescription, or as the oldLayout in an image transition.
			// When transitioning out of this layout, the contents of the memory are not guaranteed to be preserved (11.4)
		case VK_IMAGE_LAYOUT_UNDEFINED:
			if (IsDstMask)
			{
				CHECK_WITH_LOG(true, "The new layout used in a transition must not be VK_IMAGE_LAYOUT_UNDEFINED. "
					"This layout must only be used as the initialLayout member of VkImageCreateInfo "
					"or VkAttachmentDescription, or as the oldLayout in an image transition. (11.4)");
			}
			break;

			// supports all types of device access
		case VK_IMAGE_LAYOUT_GENERAL:
			// VK_IMAGE_LAYOUT_GENERAL must be used for image load/store operations (13.1.1, 13.2.4)
			AccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			break;

			// must only be used as a color or resolve attachment in a VkFramebuffer (11.4)
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			AccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;

			// must only be used as a depth/stencil attachment in a VkFramebuffer (11.4)
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

			// must only be used as a read-only depth/stencil attachment in a VkFramebuffer and/or as a read-only image in a shader (11.4)
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			break;

			// must only be used as a read-only image in a shader (which can be read as a sampled image,
			// combined image/sampler and/or input attachment) (11.4)
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			AccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
			break;

			//  must only be used as a source image of a transfer command (11.4)
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			AccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;

			// must only be used as a destination image of a transfer command (11.4)
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			AccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;

			// does not support device access. This layout must only be used as the initialLayout member
			// of VkImageCreateInfo or VkAttachmentDescription, or as the oldLayout in an image transition.
			// When transitioning out of this layout, the contents of the memory are preserved. (11.4)
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			if (!IsDstMask)
			{
				AccessMask = VK_ACCESS_HOST_WRITE_BIT;
			}
			else
			{
				CHECK_WITH_LOG(true,"The new layout used in a transition must not be VK_IMAGE_LAYOUT_PREINITIALIZED. "
					"This layout must only be used as the initialLayout member of VkImageCreateInfo "
					"or VkAttachmentDescription, or as the oldLayout in an image transition. (11.4)");
			}
			break;

		case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
			AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
			AccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			break;

			// When transitioning the image to VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR or VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			// there is no need to delay subsequent processing, or perform any visibility operations (as vkQueuePresentKHR
			// performs automatic visibility operations). To achieve this, the dstAccessMask member of the VkImageMemoryBarrier
			// should be set to 0, and the dstStageMask parameter should be set to VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT.
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			AccessMask = 0;
			break;

		case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
			AccessMask = VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
			break;

		case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
			AccessMask = VK_ACCESS_FRAGMENT_DENSITY_MAP_READ_BIT_EXT;
			break;

		default:
			CHECK_WITH_LOG(true, "Unknown image layout");
			break;
		}

		return AccessMask;
	}

	/*
	void VK_Utils::Destroy_Buffer(VK_GraphicsContext* context, AllocatedBufferUntyped& buffer)
	{
		context->destroy_allocate_buffer(buffer);
	}

	void* VK_Utils::Map_Buffer(VK_GraphicsContext* context, AllocatedBufferUntyped& buffer)
	{
		return context->map_allocate_buffer(buffer);
	}

	void VK_Utils::Unmap_Buffer(VK_GraphicsContext* context, AllocatedBufferUntyped& buffer)
	{
		return context->unmap_allocate_buffer(buffer);
	}

	VkImageViewCreateInfo VK_Utils::Imageview_Create_Info(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.pNext = nullptr;

		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.image = image;
		info.format = format;
		info.subresourceRange.baseMipLevel = 0;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;
		info.subresourceRange.aspectMask = aspectFlags;

		return info;
	}

	VkImageCreateInfo VK_Utils::Image_Create_Info(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent)
	{
		VkImageCreateInfo info = { };
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.pNext = nullptr;

		info.imageType = VK_IMAGE_TYPE_2D;

		info.format = format;
		info.extent = extent;

		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = usageFlags;

		return info;
	}

	void VK_Utils::Immediate_Submit(VK_GraphicsContext* context, std::function<void(VkCommandBuffer cmd)>&& function)
	{
		VkCommandBuffer commandBuffer = context->begin_single_time_commands();
		function(commandBuffer);
		context->end_single_time_commands(commandBuffer);
	}

	MXRender::AllocatedImage VK_Utils::Upload_Image_Mipmapped(VK_GraphicsContext* context, int texWidth, int texHeight, VkFormat image_format, AllocatedBufferUntyped& stagingBuffer, std::vector<MipmapInfo> mips)
	{
		return context->upload_allocate_image(context,texWidth,texHeight,image_format,stagingBuffer,mips);
	}

	bool VK_Utils::Load_Image_From_Asset(VK_GraphicsContext* context, const char* filename, AllocatedImage& outImage)
	{
		assets::AssetFile file;
		bool loaded = assets::load_binaryfile(filename, file);

		if (!loaded) {
			std::cout << "Error when loading texture " << filename << std::endl;
			return false;
		}

		assets::TextureInfo textureInfo = assets::read_texture_info(&file);


		VkDeviceSize imageSize = textureInfo.textureSize;
		VkFormat image_format;
		switch (textureInfo.textureFormat) {
		case assets::TextureFormat::RGBA8:
			image_format = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		default:
			return false;
		}

		AllocatedBufferUntyped stagingBuffer = Create_buffer(context,imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_UNKNOWN, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

		std::vector<MipmapInfo> mips;

		void* data=Map_Buffer(context,stagingBuffer);
		size_t offset = 0;
		{

			for (int i = 0; i < textureInfo.pages.size(); i++) {
				MipmapInfo mip;
				mip.dataOffset = offset;
				mip.dataSize = textureInfo.pages[i].originalSize;
				mips.push_back(mip);
				assets::unpack_texture_page(&textureInfo, i, file.binaryBlob.data(), (char*)data + offset);

				offset += mip.dataSize;
			}
		}
		Unmap_Buffer(context,stagingBuffer);

		outImage = Upload_Image_Mipmapped(context,textureInfo.pages[0].width, textureInfo.pages[0].height, image_format, stagingBuffer, mips);

		Destroy_Buffer(context,stagingBuffer);

		return true;
	}
	*/
	VkSamplerCreateInfo VK_Utils::Sampler_Create_Info(VkFilter filters, VkSamplerAddressMode samplerAdressMode)
	{
		VkSamplerCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.pNext = nullptr;

		info.magFilter = filters;
		info.minFilter = filters;
		info.addressModeU = samplerAdressMode;
		info.addressModeV = samplerAdressMode;
		info.addressModeW = samplerAdressMode;

		return info;
	}

	VkPipelineDepthStencilStateCreateInfo VK_Utils::Depth_Stencil_Create_Info(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp)
	{
		VkPipelineDepthStencilStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.depthTestEnable = bDepthTest ? VK_TRUE : VK_FALSE;
		info.depthWriteEnable = bDepthWrite ? VK_TRUE : VK_FALSE;
		info.depthCompareOp = bDepthTest ? compareOp : VK_COMPARE_OP_ALWAYS;
		info.depthBoundsTestEnable = VK_FALSE;
		info.minDepthBounds = 0.0f; // Optional
		info.maxDepthBounds = 1.0f; // Optional
		info.stencilTestEnable = VK_FALSE;

		return info;
	}

	VkPipelineColorBlendAttachmentState VK_Utils::Color_Blend_Attachment_State()
	{
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		return colorBlendAttachment;
	}

	VkPipelineMultisampleStateCreateInfo VK_Utils::Multisampling_State_Create_Info()
	{
		VkPipelineMultisampleStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.sampleShadingEnable = VK_FALSE;
		//multisampling defaulted to no multisampling (1 sample per pixel)
		info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		info.minSampleShading = 1.0f;
		info.pSampleMask = nullptr;
		info.alphaToCoverageEnable = VK_FALSE;
		info.alphaToOneEnable = VK_FALSE;
		return info;
	}

	VkPipelineInputAssemblyStateCreateInfo VK_Utils::Input_Assembly_Create_Info(VkPrimitiveTopology topology)
	{
		VkPipelineInputAssemblyStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.topology = topology;
		//we are not going to use primitive restart on the entire tutorial so leave it on false
		info.primitiveRestartEnable = VK_FALSE;
		return info;
	}

	VkPipelineRasterizationStateCreateInfo VK_Utils::Rasterization_State_Create_Info(VkPolygonMode polygonMode)
	{
		VkPipelineRasterizationStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.depthClampEnable = VK_FALSE;
		//rasterizer discard allows objects with holes, default to no
		info.rasterizerDiscardEnable = VK_FALSE;

		info.polygonMode = polygonMode;
		info.lineWidth = 1.0f;
		//no backface cull
		info.cullMode = VK_CULL_MODE_BACK_BIT;
		info.frontFace = VK_FRONT_FACE_CLOCKWISE;
		//no depth bias
		info.depthBiasEnable = VK_FALSE;
		info.depthBiasConstantFactor = 0.0f;
		info.depthBiasClamp = 0.0f;
		info.depthBiasSlopeFactor = 0.0f;

		return info;
	}

	VkPipelineShaderStageCreateInfo VK_Utils::Pipeline_Shader_Stage_Create_Info(VkShaderStageFlagBits stage, VkShaderModule shaderModule)
	{
		VkPipelineShaderStageCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.pNext = nullptr;

		//shader stage
		info.stage = stage;
		//module containing the code for this shader stage
		info.module = shaderModule;
		//the entry point of the shader
		info.pName = "main";
		return info;
	}

	VkPipelineVertexInputStateCreateInfo VK_Utils::Vertex_Input_State_Create_Info()
	{
		VkPipelineVertexInputStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		info.pNext = nullptr;

		//no vertex bindings or attributes
		info.vertexBindingDescriptionCount = 0;
		info.vertexAttributeDescriptionCount = 0;
		return info;
	}

	VkPipelineLayoutCreateInfo VK_Utils::Pipeline_Layout_Create_Info()
	{
		VkPipelineLayoutCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.pNext = nullptr;

		//empty defaults
		info.flags = 0;
		info.setLayoutCount = 0;
		info.pSetLayouts = nullptr;
		info.pushConstantRangeCount = 0;
		info.pPushConstantRanges = nullptr;
		return info;
	}
	VkSampler VK_Utils::Create_Linear_Sampler(VkPhysicalDevice physical_device, VkDevice device)
	{
		VkSampler sampler;
		VkPhysicalDeviceProperties physical_device_properties{};
		vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);

		VkSamplerCreateInfo samplerInfo{};

		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = physical_device_properties.limits.maxSamplerAnisotropy; // close :1.0f
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 8.0f; // todo: m_irradiance_texture_miplevels
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;

		CHECK_WITH_LOG((vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS), "vk create sampler");

		return sampler;
	}


	VkSampleCountFlagBits VK_Utils::Get_SampleCountFlagBits_FromInt(unsigned num)
	{
        if (num >= 0&&num<2)
        {
            return VK_SAMPLE_COUNT_1_BIT;
        }
        else if ( num >= 2&&num<4)
        {
            return VK_SAMPLE_COUNT_2_BIT;
        }
        else if (num >=4 && num < 8)
        {
            return VK_SAMPLE_COUNT_8_BIT;
        }
        else if (num >= 8 && num < 16)
        {
            return VK_SAMPLE_COUNT_16_BIT;
        }
        else if (num >= 16 && num < 32)
        {
            return VK_SAMPLE_COUNT_32_BIT;
        }
        else if (num >= 16 && num < 32)
        {
           return VK_SAMPLE_COUNT_64_BIT;
        }
        else
        {
            return VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;
        }

	}
	VkBufferUsageFlags VK_Utils::Translate_Buffer_usage_type_To_VulkanUsageFlag(const ENUM_BUFFER_TYPE& usage_type)
	{
		// Static/Dynamic are residency modifiers, not usages: strip them so
		// combos like Storage|Dynamic (host-visible storage) translate correctly.
		ENUM_BUFFER_TYPE base_type = usage_type & ~(ENUM_BUFFER_TYPE::Dynamic | ENUM_BUFFER_TYPE::Static);
		// Usage bits are accumulated so combinations translate correctly
		// (e.g. Storage|Indirect for GPU-driven args written by compute).
		// Staging carries only the transfer bits shared by every buffer.
		CHECK_WITH_LOG(base_type == ENUM_BUFFER_TYPE::None, "RHI Error: usage type error");
		VkBufferUsageFlags usage_flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		if (EnumHasAnyFlags(base_type, ENUM_BUFFER_TYPE::Index))
			usage_flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		if (EnumHasAnyFlags(base_type, ENUM_BUFFER_TYPE::Vertex))
			usage_flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		if (EnumHasAnyFlags(base_type, ENUM_BUFFER_TYPE::Uniform))
			usage_flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		if (EnumHasAnyFlags(base_type, ENUM_BUFFER_TYPE::Storage))
			usage_flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		if (EnumHasAnyFlags(base_type, ENUM_BUFFER_TYPE::Indirect))
			usage_flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
		return usage_flags;
	}

	VkImageLayout VK_Utils::Translate_Texture_usage_type_To_Vulkan(const ENUM_TEXTURE_USAGE_TYPE& usage_type)
	{
        switch (usage_type)
        {
		case  ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_COLOR_ATTACHMENT:
			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			break;
		case ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_PRESENT_SWAPCHAIN:
			return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			break;
		default:
			return VK_IMAGE_LAYOUT_UNDEFINED;
			break;
        }
	}

	VkImageUsageFlags VK_Utils::Translate_Texture_usage_type_To_VulkanUsageFlags(const ENUM_TEXTURE_USAGE_TYPE& usage_type)
	{
		VkImageUsageFlags usege_flags = 0;
		if (EnumHasAnyFlags(usage_type, ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_COLOR_ATTACHMENT))
		{
			usege_flags = usege_flags | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if (EnumHasAnyFlags(usage_type, ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_PRESENT_SWAPCHAIN))
		{
			usege_flags = usege_flags | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if (EnumHasAnyFlags(usage_type, ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_DEPTH_ATTACHMENT))
		{
			usege_flags = usege_flags | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if (EnumHasAnyFlags(usage_type, ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_DEPTH_ATTACHMENT_READ_ONLY))
		{
			usege_flags = usege_flags | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if (EnumHasAnyFlags(usage_type, ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_DEPTH_ATTACHMENT_WRITE_ONLY))
		{
			usege_flags = usege_flags | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		}
		if (EnumHasAnyFlags(usage_type, ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE))
		{
			usege_flags = usege_flags | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if (EnumHasAnyFlags(usage_type, ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_COPY))
		{
			usege_flags = usege_flags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		if (EnumHasAnyFlags(usage_type, ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_STORAGE))
		{
			usege_flags = usege_flags | VK_IMAGE_USAGE_STORAGE_BIT;
		}
		return usege_flags;
		for (Int i = 0; i < 32; i++)
		{
			if ((UInt32)(usage_type) & (1 << i))
			{
				ENUM_TEXTURE_USAGE_TYPE temp_usage_type = (ENUM_TEXTURE_USAGE_TYPE)(((UInt32)(usage_type) & (1 << i)));
				switch (temp_usage_type)
				{
				case  ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_COLOR_ATTACHMENT:
					usege_flags = VkImageUsageFlagBits(usege_flags | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
					break;
				case ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_PRESENT_SWAPCHAIN:
					usege_flags = usege_flags | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
					break;
				case ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_DEPTH_ATTACHMENT:
					usege_flags = usege_flags | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
					break;
				case ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_DEPTH_ATTACHMENT_READ_ONLY:
					usege_flags = usege_flags | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
					break;
				case ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_DEPTH_ATTACHMENT_WRITE_ONLY:
					usege_flags = usege_flags | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
					break;
				case ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE:
					usege_flags = usege_flags | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
					break;
				case ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_COPY:
					usege_flags = usege_flags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
					break;
				default:
					usege_flags = usege_flags | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
					CHECK_WITH_LOG(true, "RHI Error: usage type error");
					break;
				}
			}
		}
		return usege_flags;
	}

	
	VkImageType VK_Utils::Translate_Texture_type_To_Vulkan(const ENUM_TEXTURE_TYPE& type)
	{
        switch (type)
        {
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D:
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_MULTISAMPLE:
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_DEPTH:
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_CUBE_MAP:
		{
            return VK_IMAGE_TYPE_2D;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_3D:
		{
			return VK_IMAGE_TYPE_3D;
			break;
		}
        default:
            return VK_IMAGE_TYPE_MAX_ENUM;
            break;
        }
	}

	VkFormat VK_Utils::Translate_Texture_Format_To_Vulkan(const ENUM_TEXTURE_FORMAT& format) 
	{
		VkFormat vulkan_image_format = VK_FORMAT_UNDEFINED;
		switch (format)
		{
		case ENUM_TEXTURE_FORMAT::RGBA8S:
		{
			vulkan_image_format = VK_FORMAT_R8G8B8A8_SRGB;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGBA8:
		{
			vulkan_image_format = VK_FORMAT_R8G8B8A8_UNORM;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGBA16S:
		case ENUM_TEXTURE_FORMAT::RGBA16F:
		{
			vulkan_image_format = VK_FORMAT_R16G16B16A16_SFLOAT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGBA32F:
		{
			vulkan_image_format = VK_FORMAT_R32G32B32A32_SFLOAT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::R32F:
		{
			vulkan_image_format = VK_FORMAT_R32_SFLOAT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::R16F:
		{
			vulkan_image_format = VK_FORMAT_R16_SFLOAT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::R32U:
		{
			vulkan_image_format = VK_FORMAT_R32_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::R32I:
		{
			vulkan_image_format = VK_FORMAT_R32_SINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::R8:
		{
			vulkan_image_format = VK_FORMAT_R8_UNORM;
			break;
		}
		case ENUM_TEXTURE_FORMAT::R8S:
		{
			vulkan_image_format = VK_FORMAT_R8_SRGB;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RG8:
		{
			vulkan_image_format = VK_FORMAT_R8G8_UNORM;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RG8S:
		{
			vulkan_image_format = VK_FORMAT_R8G8_SRGB;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RG16F:
		{
			vulkan_image_format = VK_FORMAT_R16G16_SFLOAT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RG32F:
		{
			vulkan_image_format = VK_FORMAT_R32G32_SFLOAT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RG8I:
		{
			vulkan_image_format = VK_FORMAT_R8G8_SINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RG8U:
		{
			vulkan_image_format = VK_FORMAT_R8G8_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RG16I:
		{
			vulkan_image_format = VK_FORMAT_R16G16_SINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RG16U:
		{
			vulkan_image_format = VK_FORMAT_R16G16_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RG32I:
		{
			vulkan_image_format = VK_FORMAT_R32G32_SINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RG32U:
		{
			vulkan_image_format = VK_FORMAT_R32G32_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGB8:
		{
			vulkan_image_format = VK_FORMAT_R8G8B8_UNORM;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGB8S:
		{
			vulkan_image_format = VK_FORMAT_R8G8B8_SRGB;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGB16F:
		{
			vulkan_image_format = VK_FORMAT_R16G16B16_SFLOAT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGB32F:
		{
			vulkan_image_format = VK_FORMAT_R32G32B32_SFLOAT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGB8I:
		{
			vulkan_image_format = VK_FORMAT_R8G8B8_SINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGB8U:
		{
			vulkan_image_format = VK_FORMAT_R8G8B8_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGB16I:
		{
			vulkan_image_format = VK_FORMAT_R16G16B16_SINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGB16U:
		{
			vulkan_image_format = VK_FORMAT_R16G16B16_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGB32I:
		{
			vulkan_image_format = VK_FORMAT_R32G32B32_SINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGB32U:
		{
			vulkan_image_format = VK_FORMAT_R32G32B32_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGBA8I:
		{
			vulkan_image_format = VK_FORMAT_R8G8B8A8_SINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGBA8U:
		{
			vulkan_image_format = VK_FORMAT_R8G8B8A8_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGBA16I:
		{
			vulkan_image_format = VK_FORMAT_R16G16B16A16_SINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGBA16U:
		{
			vulkan_image_format = VK_FORMAT_R16G16B16A16_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGBA32I:
		{
			vulkan_image_format = VK_FORMAT_R32G32B32A32_SINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::RGBA32U:
		{
			vulkan_image_format = VK_FORMAT_R32G32B32A32_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::D16:
		{
			vulkan_image_format = VK_FORMAT_D16_UNORM;
			break;
		}
		case ENUM_TEXTURE_FORMAT::D24:
		{
			vulkan_image_format = VK_FORMAT_X8_D24_UNORM_PACK32;
			break;
		}
		case ENUM_TEXTURE_FORMAT::D32:
		{
			vulkan_image_format = VK_FORMAT_D32_SFLOAT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::D32F:
		{
			vulkan_image_format = VK_FORMAT_D32_SFLOAT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::D24S8:
		{
			vulkan_image_format = VK_FORMAT_D24_UNORM_S8_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::D32FS8:
		{
			vulkan_image_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			break;
		}
		case ENUM_TEXTURE_FORMAT::BC1:
		{
			vulkan_image_format = VK_FORMAT_BC1_RGB_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::BC1A:
		{
			vulkan_image_format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::BC2:
		{
			vulkan_image_format = VK_FORMAT_BC2_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::BC3:
		{
			vulkan_image_format = VK_FORMAT_BC3_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::BC4:
		{
			vulkan_image_format = VK_FORMAT_BC4_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::BC5:
		{
			vulkan_image_format = VK_FORMAT_BC5_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::BC6H:
		{
			vulkan_image_format = VK_FORMAT_BC6H_UFLOAT_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::BC7:
		{
			vulkan_image_format = VK_FORMAT_BC7_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::ETC2:
		{
			vulkan_image_format = VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::ETC2A:
		{
			vulkan_image_format = VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::ETC2A1:
		{
			vulkan_image_format = VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::ASTC4x4:
		{
			vulkan_image_format = VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::ASTC5x5:
		{
			vulkan_image_format = VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::ASTC6x6:
		{
			vulkan_image_format = VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::ASTC8x5:
		{
			vulkan_image_format = VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::ASTC8x6:
		{
			vulkan_image_format = VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::ASTC10x5:
		{
			vulkan_image_format = VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
			break;
		}
		case ENUM_TEXTURE_FORMAT::BGRA8:
		{
			vulkan_image_format = VK_FORMAT_B8G8R8A8_UNORM;
			break;
		}
		default:
		{
			CHECK_WITH_LOG(true,"RHI Error : invalid texture_format !")
			break;
		}
		}
		return vulkan_image_format;
	}

	MXRender::ENUM_TEXTURE_FORMAT VK_Utils::Translate_Vulkan_Texture_Format_To_Common(CONST VkFormat& format)
	{
		ENUM_TEXTURE_FORMAT texture_format = ENUM_TEXTURE_FORMAT::RGBA8;
		switch (format)
		{
		case VK_FORMAT_R8G8B8A8_SRGB:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGBA8S;
			break;
		}
		case VK_FORMAT_R8G8B8A8_UNORM:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGBA8;
			break;
		}
		case VK_FORMAT_R16G16B16A16_SFLOAT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGBA16F;
			break;
		}
		case VK_FORMAT_R32G32B32A32_SFLOAT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGBA32F;
			break;
		}
		case VK_FORMAT_R32_SFLOAT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::R32F;
			break;
		}
		case VK_FORMAT_R16_SFLOAT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::R16F;
			break;
		}
		case VK_FORMAT_R8_UNORM:
		{
			texture_format = ENUM_TEXTURE_FORMAT::R8;
			break;
		}
		case VK_FORMAT_R8_SRGB:
		{
			texture_format = ENUM_TEXTURE_FORMAT::R8S;
			break;
		}
		case VK_FORMAT_R8G8_UNORM:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RG8;
			break;
		}
		case VK_FORMAT_R8G8_SRGB:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RG8S;
			break;
		}
		case VK_FORMAT_R16G16_SFLOAT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RG16F;
			break;
		}
		case VK_FORMAT_R32G32_SFLOAT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RG32F;
			break;
		}
		case VK_FORMAT_R8G8_SINT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RG8I;
			break;
		}
		case VK_FORMAT_R8G8_UINT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RG8U;
			break;
		}
		case VK_FORMAT_R16G16_SINT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RG16I;
			break;
		}
		case VK_FORMAT_R16G16_UINT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RG16U;
			break;
		}
		case VK_FORMAT_R32G32_SINT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RG32I;
			break;
		}
		case VK_FORMAT_R32G32_UINT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RG32U;
			break;
		}
		case VK_FORMAT_R8G8B8_UNORM:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGB8;
			break;
		}
		case VK_FORMAT_R8G8B8_SRGB:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGB8S;
			break;
		}
		case VK_FORMAT_R16G16B16_SFLOAT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGB16F;
			break;
		}
		case VK_FORMAT_R32G32B32_SFLOAT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGB32F;
			break;
		}
		case VK_FORMAT_R8G8B8_SINT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGB8I;
			break;
		}
		case VK_FORMAT_R8G8B8_UINT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGB8U;
			break;
		}
		case VK_FORMAT_R16G16B16_SINT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGB16I;
			break;
		}
		case VK_FORMAT_R16G16B16_UINT:
		{
			texture_format = ENUM_TEXTURE_FORMAT::RGB16U;
			break;
		}
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_B8G8R8A8_UNORM:
		{
			texture_format = ENUM_TEXTURE_FORMAT::BGRA8;
			break;
		}
		default:
		{
			CHECK_WITH_LOG(true, "RHI Error : invalid vulkan_texture_format !")
			break;
		}
		}
		return texture_format;
	}

	VkImageViewType VK_Utils::Translate_Texture_type_To_VulkanImageViewType(const ENUM_TEXTURE_TYPE& type)
	{
		VkImageViewType vulkan_image_view_type = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
		switch (type)
		{
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D:
		{
			vulkan_image_view_type = VK_IMAGE_VIEW_TYPE_2D;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_MULTISAMPLE:
		{
			vulkan_image_view_type = VK_IMAGE_VIEW_TYPE_2D;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_DEPTH:
		{
			vulkan_image_view_type = VK_IMAGE_VIEW_TYPE_2D;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_CUBE_MAP:
		{
			vulkan_image_view_type = VK_IMAGE_VIEW_TYPE_CUBE;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_ARRAY:
		{
			vulkan_image_view_type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_3D:
		{
			vulkan_image_view_type = VK_IMAGE_VIEW_TYPE_3D;
			break;
		}
		default:
		{
			CHECK_WITH_LOG(true, "RHI Error : invalid image view type !")
			break;
		}
		}
		return vulkan_image_view_type;
	}

	VkImageAspectFlags VK_Utils::Translate_Texture_type_To_VulkanImageAspectFlags(const ENUM_TEXTURE_TYPE& type)
	{
		VkImageAspectFlags vulkan_image_aspect_flags = 0;
		switch (type)
		{
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D:
		{
			vulkan_image_aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_MULTISAMPLE:
		{
			vulkan_image_aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_DEPTH:
		{
			vulkan_image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_CUBE_MAP:
		{
			vulkan_image_aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_ARRAY:
		{
			vulkan_image_aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_3D:
		{
			vulkan_image_aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
			break;
		}
		default:
		{
			CHECK_WITH_LOG(true, "RHI Error : invalid image view type !")
			break;
		}
		}
		return vulkan_image_aspect_flags;
	}

	VkImageCreateFlags VK_Utils::Translate_Texture_type_To_VulkanCreateFlags(const ENUM_TEXTURE_TYPE& type)
	{
		VkImageCreateFlags vulkan_image_create_flags = 0;
		switch (type)
		{
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D:
		{
			vulkan_image_create_flags = 0;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_MULTISAMPLE:
		{
			vulkan_image_create_flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_DEPTH:
		{
			vulkan_image_create_flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_CUBE_MAP:
		{
			vulkan_image_create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_2D_ARRAY:
		{
			vulkan_image_create_flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
			break;
		}
		case ENUM_TEXTURE_TYPE::ENUM_TYPE_3D:
		{
			vulkan_image_create_flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
			break;
		}
		default:
		{
			CHECK_WITH_LOG(true, "RHI Error : invalid texture_type !")
			break;
		}
		}
		return vulkan_image_create_flags;
	}

	VkSampleCountFlagBits VK_Utils::Translate_Texture_SampleCount_To_Vulkan(const UInt8& sample_count)
	{
		VkSampleCountFlagBits vulkan_sample_count = VK_SAMPLE_COUNT_1_BIT;
		if ((sample_count & 64) == 64)
		{
			vulkan_sample_count = VK_SAMPLE_COUNT_64_BIT;
		}
		else if ((sample_count & 32) == 32)
		{
			vulkan_sample_count = VK_SAMPLE_COUNT_32_BIT;
		}
		else if ((sample_count & 16) == 16)
		{
			vulkan_sample_count = VK_SAMPLE_COUNT_16_BIT;
		}
		else if ((sample_count & 8) == 8)
		{
			vulkan_sample_count = VK_SAMPLE_COUNT_8_BIT;
		}
		else if ((sample_count & 4) == 4)
		{
			vulkan_sample_count = VK_SAMPLE_COUNT_4_BIT;
		}
		else if ((sample_count & 2) == 2)
		{
			vulkan_sample_count = VK_SAMPLE_COUNT_2_BIT;
		}
		else if ((sample_count & 1) == 1)
		{
			vulkan_sample_count = VK_SAMPLE_COUNT_1_BIT;
		}
		else
		{
			CHECK_WITH_LOG(true, "RHI Error : invalid sample_count !")
		}
		return vulkan_sample_count;
	}

	VkAttachmentStoreOp VK_Utils::Translate_AttachmentStore_To_Vulkan(CONST ENUM_RENDERPASS_ATTACHMENT_STORE_OP& store_op)
	{
		VkAttachmentStoreOp vulkan_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		switch (store_op)
		{
		case ENUM_RENDERPASS_ATTACHMENT_STORE_OP::Store:
			vulkan_store_op = VK_ATTACHMENT_STORE_OP_STORE;
			break;
		case ENUM_RENDERPASS_ATTACHMENT_STORE_OP::DISCARD:
			vulkan_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			break;
		}
		return vulkan_store_op;

	}

	VkAttachmentLoadOp VK_Utils::Translate_AttachmentLoad_To_Vulkan(CONST ENUM_RENDERPASS_ATTACHMENT_LOAD_OP& load_op)
	{
		VkAttachmentLoadOp vulkan_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		switch (load_op)
		{
		case ENUM_RENDERPASS_ATTACHMENT_LOAD_OP::Load:
			vulkan_load_op = VK_ATTACHMENT_LOAD_OP_LOAD;
			break;
		case ENUM_RENDERPASS_ATTACHMENT_LOAD_OP::Clear:
			vulkan_load_op = VK_ATTACHMENT_LOAD_OP_CLEAR;
			break;
		case ENUM_RENDERPASS_ATTACHMENT_LOAD_OP::DISCARD:
			vulkan_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			break;
		}
		return vulkan_load_op;
	}

	VkImageLayout VK_Utils::Translate_ReourceState_To_VulkanImageLayout(CONST ENUM_RESOURCE_STATE& state, bool Is_inside_renderpass , bool frag_density_map_instead_of_shadingrate)
	{
		VkImageLayout vulkan_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (state == ENUM_RESOURCE_STATE::Invalid)
			return vulkan_image_layout;
		switch (state)
		{
		case ENUM_RESOURCE_STATE::Undefined:
			vulkan_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
			break;
		case ENUM_RESOURCE_STATE::VertexBuffer:
		case ENUM_RESOURCE_STATE::IndexBuffer:
		case ENUM_RESOURCE_STATE::ConstantBuffer:
		case ENUM_RESOURCE_STATE::StreamOut:
		case ENUM_RESOURCE_STATE::IndirectArgument:
		case ENUM_RESOURCE_STATE::BuildAsRead:
		case ENUM_RESOURCE_STATE::BuildAsWrite:
			CHECK_WITH_LOG(true, "RHI Error: Translate_ReourceState_To_Vulkan-- invalid resource state !");
			break;
		case ENUM_RESOURCE_STATE::RenderTarget:
			vulkan_image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			break;
		case ENUM_RESOURCE_STATE::UnorderedAccess:
			vulkan_image_layout = VK_IMAGE_LAYOUT_GENERAL;
			break;
		case ENUM_RESOURCE_STATE::DepthWrite:
			vulkan_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			break;
		case ENUM_RESOURCE_STATE::DepthRead:
			vulkan_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			break;
		case ENUM_RESOURCE_STATE::ShaderResource:
			vulkan_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			break;
		case ENUM_RESOURCE_STATE::CopyDest:
			vulkan_image_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			break;
		case ENUM_RESOURCE_STATE::CopySource:
			vulkan_image_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			break;
		case ENUM_RESOURCE_STATE::ResolveDest:
			vulkan_image_layout = Is_inside_renderpass? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			break;
		case ENUM_RESOURCE_STATE::ResolveSource:
			vulkan_image_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			break;
		case ENUM_RESOURCE_STATE::Present:
			vulkan_image_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			break;
		case ENUM_RESOURCE_STATE::Common:
			vulkan_image_layout = VK_IMAGE_LAYOUT_GENERAL;
			break;
		case ENUM_RESOURCE_STATE::ShaderRate:
			vulkan_image_layout = frag_density_map_instead_of_shadingrate ? VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT : VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
			break;
		}
		return vulkan_image_layout;
	}

	VkPipelineStageFlags VK_Utils::Translate_ReourceState_To_VulkanPipelineStage(CONST ENUM_RESOURCE_STATE& state)
	{		VkPipelineStageFlags vulkan_pipeline_stage = 0;
		switch (state)
		{
		case ENUM_RESOURCE_STATE::Undefined:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;
		case ENUM_RESOURCE_STATE::VertexBuffer:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
			break;
		case ENUM_RESOURCE_STATE::IndexBuffer:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
			break;
		case ENUM_RESOURCE_STATE::ConstantBuffer:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;
		case ENUM_RESOURCE_STATE::StreamOut:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
			break;
		case ENUM_RESOURCE_STATE::IndirectArgument:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
			break;
		case ENUM_RESOURCE_STATE::BuildAsRead:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
			break;
		case ENUM_RESOURCE_STATE::BuildAsWrite:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
			break;
		case ENUM_RESOURCE_STATE::RenderTarget:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;
		case ENUM_RESOURCE_STATE::UnorderedAccess:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;
		case ENUM_RESOURCE_STATE::DepthWrite:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;
		case ENUM_RESOURCE_STATE::DepthRead:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;
		case ENUM_RESOURCE_STATE::ShaderResource:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			break;
		case ENUM_RESOURCE_STATE::CopyDest:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case ENUM_RESOURCE_STATE::CopySource:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case ENUM_RESOURCE_STATE::ResolveDest:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case ENUM_RESOURCE_STATE::ResolveSource:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;
		case ENUM_RESOURCE_STATE::Present:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			break;
		case ENUM_RESOURCE_STATE::Common:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			break;
		case ENUM_RESOURCE_STATE::ShaderRate:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
			break;
		case ENUM_RESOURCE_STATE::Invalid:
			break;
		default:
			vulkan_pipeline_stage = VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;
			CHECK_WITH_LOG(true, "RHI Error: Translate_ReourceState_To_VulkanPipelineStage-- invalid resource state !");
			break;
		}
		return vulkan_pipeline_stage;
	}

	Bool VK_Utils::Check_ResourceState_Has_WriteAccess(ENUM_RESOURCE_STATE state)
	{
		constexpr ENUM_RESOURCE_STATE write_access_states =
			ENUM_RESOURCE_STATE::RenderTarget |
			ENUM_RESOURCE_STATE::UnorderedAccess |
			ENUM_RESOURCE_STATE::CopyDest |
			ENUM_RESOURCE_STATE::ResolveDest |
			ENUM_RESOURCE_STATE::BuildAsWrite;

		// --   Bitwise AND of multi-bit flags can never equal exactly 1; check non-zero instead
			return (state & write_access_states) != ENUM_RESOURCE_STATE::Invalid;
	}

	VkAccessFlags VK_Utils::Translate_ReourceState_To_VulkanAccess(CONST ENUM_RESOURCE_STATE& state)
	{
		// Complete per-bit access translation. The state enum is a multi-bit
		// flag (see GenerricRead), so each bit is checked independently.
		VkAccessFlags access = 0;
		if ((state & ENUM_RESOURCE_STATE::VertexBuffer) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		if ((state & ENUM_RESOURCE_STATE::IndexBuffer) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_INDEX_READ_BIT;
		if ((state & ENUM_RESOURCE_STATE::ConstantBuffer) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_UNIFORM_READ_BIT;
		if ((state & ENUM_RESOURCE_STATE::ShaderResource) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_SHADER_READ_BIT;
		if ((state & ENUM_RESOURCE_STATE::UnorderedAccess) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		if ((state & ENUM_RESOURCE_STATE::IndirectArgument) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		if ((state & ENUM_RESOURCE_STATE::RenderTarget) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		if ((state & ENUM_RESOURCE_STATE::DepthWrite) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		if ((state & ENUM_RESOURCE_STATE::DepthRead) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		if ((state & ENUM_RESOURCE_STATE::CopyDest) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_TRANSFER_WRITE_BIT;
		if ((state & ENUM_RESOURCE_STATE::CopySource) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_TRANSFER_READ_BIT;
		if ((state & ENUM_RESOURCE_STATE::BuildAsRead) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		if ((state & ENUM_RESOURCE_STATE::BuildAsWrite) != ENUM_RESOURCE_STATE::Invalid)
			access |= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
		if (access == 0)
			access = VK_ACCESS_MEMORY_READ_BIT;
		return access;
	}

	VkFormat VK_Utils::Translate_API_DataTypeEnum_To_Vulkan(ENUM_RENDER_DATA_TYPE data_type)
	{
		switch (data_type)
		{
		case MXRender::ENUM_RENDER_DATA_TYPE::Float:
            return VK_FORMAT_R32_SFLOAT;
            break;
		case MXRender::ENUM_RENDER_DATA_TYPE::Half:
			return VK_FORMAT_R16_SFLOAT;
			break;
		case MXRender::ENUM_RENDER_DATA_TYPE::Mat3:
		case MXRender::ENUM_RENDER_DATA_TYPE::Mat4:
			return VK_FORMAT_R32_SFLOAT;
			break;
		case MXRender::ENUM_RENDER_DATA_TYPE::Int:
			return VK_FORMAT_R32_SINT;
			break;
		case MXRender::ENUM_RENDER_DATA_TYPE::Uint8:
            return VK_FORMAT_R8_UINT;
		case MXRender::ENUM_RENDER_DATA_TYPE::Uint10:
			return VK_FORMAT_R16_UINT;
			break;
		case MXRender::ENUM_RENDER_DATA_TYPE::Int16:
			return VK_FORMAT_R16_SINT;
			break;
		default:
			return VK_FORMAT_UNDEFINED;
			break;
		}
		return VK_FORMAT_UNDEFINED;
	}

	VkShaderStageFlagBits VK_Utils::Translate_ShaderTypeEnum_To_Vulkan(ENUM_SHADER_STAGE shader_type)
	{
		switch (shader_type)
		{
		case MXRender::Shader_Vertex:
			return VK_SHADER_STAGE_VERTEX_BIT;
			break;
		case MXRender::Shader_Pixel:
			return VK_SHADER_STAGE_FRAGMENT_BIT;
			break;
		case MXRender::Shader_Geometry:
			return VK_SHADER_STAGE_GEOMETRY_BIT;
			break;
		case MXRender::Shader_Hull:
			return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
			break;
		case MXRender::Shader_Domain:
			return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
			break;
		case MXRender::NumStages:
			return VK_SHADER_STAGE_ALL_GRAPHICS;
			break;
		case MXRender::MaxNumSets:
			return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
			break;
		case MXRender::Shader_Compute:
			return VK_SHADER_STAGE_COMPUTE_BIT;
			break;
		case MXRender::Shader_RayGen:
			return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
			break;
		case MXRender::Shader_Miss:
			return VK_SHADER_STAGE_MISS_BIT_KHR;
			break;
		case MXRender::Shader_ClosestHit:
			return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
			break;
		case MXRender::Shader_AnyHit:
			return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
			break;
		case MXRender::Shader_Intersection:
			return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
			break;
		case MXRender::Shader_Callable:
			return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
			break;
		case MXRender::Invalid:
			return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
			break;
		default:
			return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
			break;
		}
	}

	VkPrimitiveTopology VK_Utils::Translate_PrimitiveTopology_To_Vulkan(ENUM_PRIMITIVE_TYPE topology)
	{
		switch (topology)
		{
		case MXRender::ENUM_PRIMITIVE_TYPE::PointList:
			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			break;
		case MXRender::ENUM_PRIMITIVE_TYPE::LineList:
			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			break;
		case MXRender::ENUM_PRIMITIVE_TYPE::TriangleList:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			break;
		case MXRender::ENUM_PRIMITIVE_TYPE::TriangleStrip:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			break;
		case MXRender::ENUM_PRIMITIVE_TYPE::TriangleFan:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
			break;
		case MXRender::ENUM_PRIMITIVE_TYPE::TriangleListWithAdjacency:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
			break;
		case MXRender::ENUM_PRIMITIVE_TYPE::TriangleStripWithAdjacency:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
			break;
		case MXRender::ENUM_PRIMITIVE_TYPE::PatchList:
			return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
			break;
		default:
			return VK_PRIMITIVE_TOPOLOGY_MAX_ENUM;
			break;
		}
	}

	VkCullModeFlags VK_Utils::Translate_CullMode_To_Vulkan(ENUM_RASTER_CULLMODE polygon_mode)
	{
		switch (polygon_mode)
		{
		case MXRender::ENUM_RASTER_CULLMODE::None:
			return VK_CULL_MODE_NONE;
			break;
		case MXRender::ENUM_RASTER_CULLMODE::Front:
			return VK_CULL_MODE_FRONT_BIT;
			break;
		case MXRender::ENUM_RASTER_CULLMODE::Back:
			return VK_CULL_MODE_BACK_BIT;
			break;
		default:
			return VK_CULL_MODE_NONE;
			break;
		}
	}

	VkBlendFactor VK_Utils::Translate_BlendFactor_To_Vulkan(ENUM_BLEND_FACTOR blend_factor)
	{
		switch (blend_factor)
		{
		case MXRender::ENUM_BLEND_FACTOR::ENUM_ZERO:
			return VK_BLEND_FACTOR_ZERO;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_ONE:
			return VK_BLEND_FACTOR_ONE;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_SRC_COLOR:
			return VK_BLEND_FACTOR_SRC_COLOR;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_ONE_MINUS_SRC_COLOR:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_DST_COLOR:
			return VK_BLEND_FACTOR_DST_COLOR;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_ONE_MINUS_DST_COLOR:
			return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_SRC_ALPHA:
			return VK_BLEND_FACTOR_SRC_ALPHA;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_ONE_MINUS_SRC_ALPHA:
			return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_DST_ALPHA:
			return VK_BLEND_FACTOR_DST_ALPHA;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_ONE_MINUS_DST_ALPHA:
			return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_CONSTANT_COLOR:
			return VK_BLEND_FACTOR_CONSTANT_COLOR;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_INV_CONSTANT_COLOR:
			return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_CONSTANT_ALPHA:
			return VK_BLEND_FACTOR_CONSTANT_ALPHA;
			break;
		case MXRender::ENUM_BLEND_FACTOR::ENUM_INV_CONSTANT_ALPHA:
			return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
			break;
		case MXRender::ENUM_BLEND_FACTOR::EUNUM_SRC_ALPHA_SATURATE:
			return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
			break;
		default:
			CHECK_WITH_LOG(true, "RHI Error: invalid blend factor !")
			return VK_BLEND_FACTOR_ZERO;
			break;
		}
	}

	VkBlendOp VK_Utils::Translate_BlendOp_To_Vulkan(ENUM_BLEND_EQUATION blend_op)
	{
		switch (blend_op)
		{
		case MXRender::ENUM_BLEND_EQUATION::ENUM_ADD:
			return VK_BLEND_OP_ADD;
			break;
		case MXRender::ENUM_BLEND_EQUATION::ENUM_SUB:
			return VK_BLEND_OP_SUBTRACT;
			break;
		case MXRender::ENUM_BLEND_EQUATION::ENUM_REVERSE_SUB:
			return VK_BLEND_OP_REVERSE_SUBTRACT;
			break;
		case MXRender::ENUM_BLEND_EQUATION::ENUM_MIN:
			return VK_BLEND_OP_MIN;
			break;
		case MXRender::ENUM_BLEND_EQUATION::ENUM_MAX:
			return VK_BLEND_OP_MAX;
			break;
		default:
			CHECK_WITH_LOG(true, "RHI Error: invalid blend equation !")
			return VK_BLEND_OP_ADD;
			break;
		}
	

	}

	VkCompareOp VK_Utils::Translate_CompareOp_To_Vulkan(ENUM_STENCIL_FUNCTION compare_op)
	{
		switch (compare_op)
		{
		case MXRender::ENUM_STENCIL_FUNCTION::ENUM_NEVER:
			return VK_COMPARE_OP_NEVER;
			break;
		case MXRender::ENUM_STENCIL_FUNCTION::ENUM_LESS:
			return VK_COMPARE_OP_LESS;
			break;
			case MXRender::ENUM_STENCIL_FUNCTION::ENUM_LEQUAL:
				return VK_COMPARE_OP_LESS_OR_EQUAL;
				break;
		case MXRender::ENUM_STENCIL_FUNCTION::ENUM_EQUAL:
			return VK_COMPARE_OP_EQUAL;
			break;
		case MXRender::ENUM_STENCIL_FUNCTION::ENUM_LESSOREQUAL :
			return VK_COMPARE_OP_LESS_OR_EQUAL;
			break;
		case MXRender::ENUM_STENCIL_FUNCTION::ENUM_GREATER:
			return VK_COMPARE_OP_GREATER;
			break;
		case MXRender::ENUM_STENCIL_FUNCTION::ENUM_NOT_EQUAL:
			return VK_COMPARE_OP_NOT_EQUAL;
			break;
		case MXRender::ENUM_STENCIL_FUNCTION::ENUM_GREATEROREQUAL:
			return VK_COMPARE_OP_GREATER_OR_EQUAL;
			break;
		case MXRender::ENUM_STENCIL_FUNCTION::ENUM_ALWAYS:
			return VK_COMPARE_OP_ALWAYS;
			break;
		default:
			CHECK_WITH_LOG(true, "RHI Error: invalid compare op !")
			return VK_COMPARE_OP_NEVER;
			break;
		}

	}

	VkStencilOpState VK_Utils::Translate_StencilOpState_To_Vulkan(StencilOpDesc stencil_op_state)
	{
		VkStencilOpState vulkan_stencil_op;
		vulkan_stencil_op.failOp = Translate_StencilOp_To_Vulkan(stencil_op_state.fail_op);
		vulkan_stencil_op.passOp = Translate_StencilOp_To_Vulkan(stencil_op_state.pass_op);
		vulkan_stencil_op.depthFailOp = Translate_StencilOp_To_Vulkan(stencil_op_state.depth_fail_op);
		vulkan_stencil_op.compareOp = Translate_CompareOp_To_Vulkan(stencil_op_state.func);
		return vulkan_stencil_op;

	}

	VkStencilOp VK_Utils::Translate_StencilOp_To_Vulkan(ENUM_STENCIL_OPERATIOON stencil_op)
	{
switch (stencil_op)
		{
		case MXRender::ENUM_STENCIL_OPERATIOON::ENUM_KEEP:
			return VK_STENCIL_OP_KEEP;
			break;
		case MXRender::ENUM_STENCIL_OPERATIOON::ENUM_ZERO:
			return VK_STENCIL_OP_ZERO;
			break;
		case MXRender::ENUM_STENCIL_OPERATIOON::ENUM_REPLACE:
			return VK_STENCIL_OP_REPLACE;
			break;
		case MXRender::ENUM_STENCIL_OPERATIOON::ENUM_INCREMENT_AND_CLAMP:
			return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
			break;
		case MXRender::ENUM_STENCIL_OPERATIOON::ENUM_DECREMENT_AND_CLAMP:
			return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
			break;
		case MXRender::ENUM_STENCIL_OPERATIOON::ENUM_INVERT:
			return VK_STENCIL_OP_INVERT;
			break;
		case MXRender::ENUM_STENCIL_OPERATIOON::ENUM_INCREMENT_AND_WRAP:
			return VK_STENCIL_OP_INCREMENT_AND_WRAP;
			break;
		case MXRender::ENUM_STENCIL_OPERATIOON::ENUM_DECREMENT_AND_WRAP:
			return VK_STENCIL_OP_DECREMENT_AND_WRAP;
			break;
		default:
			CHECK_WITH_LOG(true, "RHI Error: invalid stencil op !")
			return VK_STENCIL_OP_KEEP;
			break;
		}

	}

	VkVertexInputRate VK_Utils::Translation_VertexInputRate_To_Vulkan(ENUM_VERTEX_INPUTRATE input_rate)
	{
		switch (input_rate)
		{
		case MXRender::ENUM_VERTEX_INPUTRATE::PerVertex:
			return VK_VERTEX_INPUT_RATE_VERTEX;
			break;
		case MXRender::ENUM_VERTEX_INPUTRATE::PerInstance:
			return VK_VERTEX_INPUT_RATE_INSTANCE;
			break;
		default:
			CHECK_WITH_LOG(true,"RHI Error: invalid vertex input rate !")
			return VK_VERTEX_INPUT_RATE_MAX_ENUM;
			break;
		}
	}

	VkDescriptorType VK_Utils::Translate_BindingResourceType_To_VulkanDescriptorType(ENUM_BINDING_RESOURCE_TYPE bingding_resource_type)
	{
		switch (bingding_resource_type)
		{
		case MXRender::ENUM_BINDING_RESOURCE_TYPE::UniformBuffer:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			break;
		case MXRender::ENUM_BINDING_RESOURCE_TYPE::StorageBuffer:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			break;
		case MXRender::ENUM_BINDING_RESOURCE_TYPE::SampledTexture:
			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			break;
		case MXRender::ENUM_BINDING_RESOURCE_TYPE::StorageTexture:
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			break;
		case MXRender::ENUM_BINDING_RESOURCE_TYPE::Sampler:
			return VK_DESCRIPTOR_TYPE_SAMPLER;
			break;
		case MXRender::ENUM_BINDING_RESOURCE_TYPE::UniformBufferDynamic:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			break;
		case MXRender::ENUM_BINDING_RESOURCE_TYPE::StorageBufferDynamic:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
			break;
		default:
			CHECK_WITH_LOG(true, "RHI Error: invalid binding resource type !")
			break;
		}
	}

	VkPolygonMode VK_Utils::Translate_FillMode_To_Vulkan(ENUM_RASTER_FILLMODE polygon_mode)
	{
		switch (polygon_mode)
		{
		case MXRender::ENUM_RASTER_FILLMODE::Line:
			return VK_POLYGON_MODE_LINE;
			break;
		case MXRender::ENUM_RASTER_FILLMODE::Fill:
			return VK_POLYGON_MODE_FILL;
			break;
		default:
			return VK_POLYGON_MODE_MAX_ENUM;
			break;
		}

	}


	void CheckAndSetAccessMaskAndStage(VkImageLayout& old_layout, VkImageLayout& new_layout, VkImageMemoryBarrier& barrier, VkPipelineStageFlags& sourceStage, VkPipelineStageFlags& destinationStage)
	{
		if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
			new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		// for getGuidAndDepthOfMouseClickOnRenderSceneForUI() get depthimage
		else if (old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
			new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
			new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT| VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		}
		// for generating mipmapped image
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
			new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && new_layout == VK_IMAGE_LAYOUT_GENERAL)
		{
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_GENERAL && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		{
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_UNDEFINED)
		{
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.dstAccessMask = 0;

			sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_GENERAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
		barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT| VK_PIPELINE_STAGE_VERTEX_SHADER_BIT| VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL  && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;

		destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}
		else
		{
			throw std::invalid_argument("unsupported layout transition!");
		}
	}

}

