#include "VK_BindlessManager.h"
#include "VK_Device.h"
#include "VK_Texture.h"
#include "RHI/RenderTexture.h"
#include "Core/ConstDefine.h"
#include "Core/ConstGlobals.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(Vulkan)

VK_BindlessManager::VK_BindlessManager(VK_Device* in_device)
	: device(in_device)
{
	if (!device->GetOptionalExtensions().HasEXTDescriptorIndexing)
	{
		return; // Bindless not supported on this device
	}

	// Initialize slot tracking — generation-protected free list
	slot_meta_2d.resize(MAX_TEXTURES_2D);
	for (UInt32 i = 1; i < MAX_TEXTURES_2D - 1; ++i)
		slot_meta_2d[i].next_free = i + 1;
	slot_meta_2d[MAX_TEXTURES_2D - 1].next_free = 0;
	free_head_2d = 1;

	slot_meta_cube.resize(MAX_TEXTURES_CUBE);
	for (UInt32 i = 1; i < MAX_TEXTURES_CUBE - 1; ++i)
		slot_meta_cube[i].next_free = i + 1;
	slot_meta_cube[MAX_TEXTURES_CUBE - 1].next_free = 0;
	free_head_cube = 1;

	CreateBindlessPool();
	CreateBindlessLayout();
	CreateBindlessDescriptorSet();

	is_enabled = true;
}

VK_BindlessManager::~VK_BindlessManager()
{
	if (layout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device->GetDevice(), layout, nullptr);
		layout = VK_NULL_HANDLE;
	}
	if (pool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device->GetDevice(), pool, nullptr);
		pool = VK_NULL_HANDLE;
	}
	// descriptor_set is implicitly freed with pool
	descriptor_set = VK_NULL_HANDLE;
}

void VK_BindlessManager::CreateBindlessPool()
{
	VkDescriptorPoolSize pool_sizes[3] = {};
	pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes[0].descriptorCount = MAX_TEXTURES_2D + MAX_TEXTURES_CUBE;
	pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;  // for cube
	pool_sizes[1].descriptorCount = MAX_TEXTURES_CUBE;
	pool_sizes[2].type = VK_DESCRIPTOR_TYPE_SAMPLER;
	pool_sizes[2].descriptorCount = MAX_SAMPLERS;

	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	pool_info.maxSets = 1;  // Single global descriptor set
	pool_info.poolSizeCount = 3;
	pool_info.pPoolSizes = pool_sizes;

	CHECK_WITH_LOG(
		vkCreateDescriptorPool(device->GetDevice(), &pool_info, nullptr, &pool) != VK_SUCCESS,
		"RHI Error: Failed to create bindless descriptor pool");
}

void VK_BindlessManager::CreateBindlessLayout()
{
	VkDescriptorSetLayoutBinding bindings[3] = {};

	// Binding 0: 2D texture array (variable count, update-after-bind)
	bindings[0].binding = BINDLESS_TEXTURE_2D_BINDING;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = MAX_TEXTURES_2D;
	bindings[0].stageFlags = VK_SHADER_STAGE_ALL;
	bindings[0].pImmutableSamplers = nullptr;

	// Binding 1: Cube texture array (variable count, update-after-bind)
	bindings[1].binding = BINDLESS_TEXTURE_CUBE_BINDING;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = MAX_TEXTURES_CUBE;
	bindings[1].stageFlags = VK_SHADER_STAGE_ALL;
	bindings[1].pImmutableSamplers = nullptr;

	// Binding 2: Sampler array (variable count, update-after-bind)
	bindings[2].binding = BINDLESS_SAMPLER_BINDING;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	bindings[2].descriptorCount = MAX_SAMPLERS;
	bindings[2].stageFlags = VK_SHADER_STAGE_ALL;
	bindings[2].pImmutableSamplers = nullptr;

	// Per-binding flags for bindless (update-after-bind + partially bound)
	VkDescriptorBindingFlags binding_flags[3] = {};
	binding_flags[0] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
		| VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
	binding_flags[1] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
		| VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
	binding_flags[2] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
		| VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info{};
	binding_flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	binding_flags_info.bindingCount = 3;
	binding_flags_info.pBindingFlags = binding_flags;

	VkDescriptorSetLayoutCreateInfo layout_info{};
	layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layout_info.pNext = &binding_flags_info;
	layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layout_info.bindingCount = 3;
	layout_info.pBindings = bindings;

	CHECK_WITH_LOG(
		vkCreateDescriptorSetLayout(device->GetDevice(), &layout_info, nullptr, &layout) != VK_SUCCESS,
		"RHI Error: Failed to create bindless descriptor set layout");
}

void VK_BindlessManager::CreateBindlessDescriptorSet()
{
	// Allocate the descriptor set with fixed-size arrays (no variable descriptor count needed)

	VkDescriptorSetAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.descriptorPool = pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pSetLayouts = &layout;

	CHECK_WITH_LOG(
		vkAllocateDescriptorSets(device->GetDevice(), &alloc_info, &descriptor_set) != VK_SUCCESS,
		"RHI Error: Failed to allocate bindless descriptor set");
}

// RHI::Texture* overloads — cast to VK_Texture internally, return typed handle
BindlessSlotHandle VK_BindlessManager::AllocateTexture2DSlot(MXRender::RHI::Texture* in_texture)
{
	return AllocateTexture2DSlot(static_cast<VK_Texture*>(in_texture));
}

BindlessCubeSlotHandle VK_BindlessManager::AllocateTextureCubeSlot(MXRender::RHI::Texture* in_texture)
{
	return AllocateTextureCubeSlot(static_cast<VK_Texture*>(in_texture));
}

void VK_BindlessManager::UpdateTexture2DSlot(UInt32 index, MXRender::RHI::Texture* in_texture)
{
	UpdateTexture2DSlot(index, static_cast<VK_Texture*>(in_texture));
}

BindlessSlotHandle VK_BindlessManager::AllocateTexture2DSlot(VK_Texture* texture)
{
	if (!is_enabled || !texture) return BindlessSlotHandle{};

	// Recycle slots whose deferral has expired before deciding we are exhausted.
	ProcessPendingFree2D();

	if (free_head_2d == 0) return BindlessSlotHandle{}; // exhausted

	UInt32 idx = free_head_2d;
	auto REF slot = slot_meta_2d[idx];
	free_head_2d = slot.next_free;

	UInt32 gen = slot.generation;
	slot.next_free = 0;

	UpdateTexture2DSlot(idx, texture);

	return BindlessSlotHandle::Make(idx, gen);
}

void VK_BindlessManager::FreeTexture2DSlot(BindlessSlotHandle handle)
{
	if (!is_enabled) return;

	UInt32 idx = handle.GetIndex();
	UInt32 gen = handle.GetGeneration();

	if (idx == 0 || idx >= MAX_TEXTURES_2D) return;

	auto REF slot = slot_meta_2d[idx];
	if (slot.generation != gen) return; // stale handle — already freed

	// Bump generation so all existing handles to this slot become stale
	slot.generation = ((gen + 1) & kHandleGenMask);
	if (slot.generation == 0) slot.generation = 1;

	// Do NOT push the slot back onto the free list yet. Two frames can be in
	// flight, and the bindless layout sets UPDATE_AFTER_BIND but not
	// UPDATE_UNUSED_WHILE_PENDING, so reallocating now would let the next
	// vkUpdateDescriptorSets overwrite a descriptor that an in-flight command
	// buffer is still reading. Recycling is deferred instead.
	pending_free_2d.push_back({ idx, g_frame_number_render_thread.load() });
}

void VK_BindlessManager::ProcessPendingFree2D()
{
	const UInt64 now = g_frame_number_render_thread.load();
	for (Int i = static_cast<Int>(pending_free_2d.size()) - 1; i >= 0; --i)
	{
		if (pending_free_2d[i].frame + kDeferredFreeFrames < now)
		{
			const UInt32 idx = pending_free_2d[i].index;
			slot_meta_2d[idx].next_free = free_head_2d;
			free_head_2d = idx;
			pending_free_2d[i] = pending_free_2d.back();
			pending_free_2d.pop_back();
		}
	}
}

void VK_BindlessManager::UpdateTexture2DSlot(UInt32 index, VK_Texture* texture)
{
	if (!is_enabled || index >= MAX_TEXTURES_2D || !texture) return;

	VkDescriptorImageInfo image_info{};
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.imageView = texture->GetImageView();
	image_info.sampler = texture->GetSampler();

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptor_set;
	write.dstBinding = BINDLESS_TEXTURE_2D_BINDING;
	write.dstArrayElement = index;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &image_info;

	vkUpdateDescriptorSets(device->GetDevice(), 1, &write, 0, nullptr);
}

BindlessCubeSlotHandle VK_BindlessManager::AllocateTextureCubeSlot(VK_Texture* texture)
{
	if (!is_enabled || !texture) return BindlessCubeSlotHandle{};

	ProcessPendingFreeCube();

	if (free_head_cube == 0) return BindlessCubeSlotHandle{}; // exhausted

	UInt32 idx = free_head_cube;
	auto REF slot = slot_meta_cube[idx];
	free_head_cube = slot.next_free;

	UInt32 gen = slot.generation;
	slot.next_free = 0;

	VkDescriptorImageInfo image_info{};
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.imageView = texture->GetImageView();
	image_info.sampler = texture->GetSampler();

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descriptor_set;
	write.dstBinding = BINDLESS_TEXTURE_CUBE_BINDING;
	write.dstArrayElement = idx;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &image_info;

	vkUpdateDescriptorSets(device->GetDevice(), 1, &write, 0, nullptr);

	return BindlessCubeSlotHandle::Make(idx, gen);
}

void VK_BindlessManager::FreeTextureCubeSlot(BindlessCubeSlotHandle handle)
{
	if (!is_enabled) return;

	UInt32 idx = handle.GetIndex();
	UInt32 gen = handle.GetGeneration();

	if (idx == 0 || idx >= MAX_TEXTURES_CUBE) return;

	auto REF slot = slot_meta_cube[idx];
	if (slot.generation != gen) return; // stale handle

	// Bump generation
	slot.generation = ((gen + 1) & kHandleGenMask);
	if (slot.generation == 0) slot.generation = 1;

	// Deferred recycling, same reasoning as the 2D path.
	pending_free_cube.push_back({ idx, g_frame_number_render_thread.load() });
}

void VK_BindlessManager::ProcessPendingFreeCube()
{
	const UInt64 now = g_frame_number_render_thread.load();
	for (Int i = static_cast<Int>(pending_free_cube.size()) - 1; i >= 0; --i)
	{
		if (pending_free_cube[i].frame + kDeferredFreeFrames < now)
		{
			const UInt32 idx = pending_free_cube[i].index;
			slot_meta_cube[idx].next_free = free_head_cube;
			free_head_cube = idx;
			pending_free_cube[i] = pending_free_cube.back();
			pending_free_cube.pop_back();
		}
	}
}

VkDescriptorSetLayout VK_BindlessManager::GetLayout() CONST
{
	return layout;
}

VkDescriptorSet VK_BindlessManager::GetDescriptorSet() CONST
{
	return descriptor_set;
}

Bool VK_BindlessManager::IsEnabled() CONST
{
	return is_enabled;
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
