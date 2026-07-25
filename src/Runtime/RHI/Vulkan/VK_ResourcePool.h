#pragma once

#ifndef _VK_RESOURCE_POOL_
#define _VK_RESOURCE_POOL_

#include "Core/ConstDefine.h"
#include "RHI/RenderResource.h"
#include "VK_Texture.h"
#include "VK_Buffer.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

MYRENDERER_BEGIN_CLASS(VK_ResourcePool)

#pragma region METHOD
public:
	VK_ResourcePool() MYDEFAULT;
	~VK_ResourcePool() MYDEFAULT;

	// Try to acquire a pooled texture. Returns nullptr if no match.
	std::unique_ptr<VK_Texture> AcquireTexture(CONST TextureDesc& desc);
	// Return a texture to the pool. Takes ownership.
	void ReturnTexture(std::unique_ptr<VK_Texture> texture, CONST TextureDesc& desc);
	// Same for buffers
	std::unique_ptr<VK_Buffer> AcquireBuffer(CONST BufferDesc& desc);
	void ReturnBuffer(std::unique_ptr<VK_Buffer> buffer, CONST BufferDesc& desc);

private:
	static UInt64 HashTextureDesc(CONST TextureDesc& desc);
	static UInt64 HashBufferDesc(CONST BufferDesc& desc);
	static Bool TextureDescMatch(CONST TextureDesc& a, CONST TextureDesc& b);
	static Bool BufferDescMatch(CONST BufferDesc& a, CONST BufferDesc& b);

#pragma endregion

#pragma region MEMBER
private:
	// Pool entries grouped by description hash
	Map<UInt64, Vector<std::unique_ptr<VK_Texture>>> texture_pool;
	Map<UInt64, Vector<std::unique_ptr<VK_Buffer>>>  buffer_pool;
	static constexpr UInt32 MaxPooledPerKey = 8;  // cap per desc
#pragma endregion

MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif
