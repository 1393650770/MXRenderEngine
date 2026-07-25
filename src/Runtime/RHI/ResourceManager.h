#pragma once
#ifndef _RESOURCEMANAGER_
#define _RESOURCEMANAGER_

#include "Core/ConstDefine.h"
#include "Core/ResourceHandle.h"
#include "Core/ResourceRegistry.h"
#include "RHIHandleTypes.h"
#include "RenderResource.h"
#include "RenderTexture.h"
#include "RenderBuffer.h"
#include "RenderShader.h"
#include "RenderPipelineState.h"
#include "RenderRHI.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)

// Handle type traits — maps Handle type to raw pointer, descriptor, and creation function.
template<typename H> struct HandleTraits;
template<> struct HandleTraits<TextureHandle> {
	using Ptr = Texture*;
	using Desc = TextureDesc;
	using RegistryType = Texture;
	static Ptr CreateRaw(const Desc& d) { return g_render_rhi->CreateTexture(d); }
};
template<> struct HandleTraits<BufferHandle> {
	using Ptr = Buffer*;
	using Desc = BufferDesc;
	using RegistryType = Buffer;
	static Ptr CreateRaw(const Desc& d) { return g_render_rhi->CreateBuffer(d); }
};
template<> struct HandleTraits<PSOHandle> {
	using Ptr = RenderPipelineState*;
	using Desc = RenderGraphiPipelineStateDesc;
	using RegistryType = RenderPipelineState;
	static Ptr CreateRaw(const Desc& d) { return g_render_rhi->CreateRenderPipelineState(d); }
};

MYRENDERER_BEGIN_CLASS_WITH_DERIVE(ResourceManager, public RenderResource)
#pragma region METHOD
public:
	ResourceManager();
	VIRTUAL ~ResourceManager() MYDEFAULT;

	template<typename H>
	H Create(const typename HandleTraits<H>::Desc& desc, const String& name) {
		auto* raw = HandleTraits<H>::CreateRaw(desc);
		if (!raw) return H{};
		auto& reg = GetRegistry<typename HandleTraits<H>::RegistryType>();
		GenericHandle h = reg.Allocate(raw, name);
		return H{h};
	}
	template<typename H>
	H Import(typename HandleTraits<H>::Ptr existing, const String& name) {
		if (!existing) return H{};
		auto& reg = GetRegistry<typename HandleTraits<H>::RegistryType>();
		GenericHandle h = reg.Allocate(existing, name);
		return H{h};
	}
	template<typename H>
	typename HandleTraits<H>::Ptr Resolve(H h) const {
		auto& reg = const_cast<ResourceManager*>(this)->GetRegistry<typename HandleTraits<H>::RegistryType>();
		return reg.Resolve(h.value);
	}
	template<typename H>
	void Destroy(H h) {
		if (!h.IsValid()) return;
		auto& reg = GetRegistry<typename HandleTraits<H>::RegistryType>();
		auto* raw = reg.Free(h.value);
		if (!raw) return;
		delete raw;
	}
	template<typename T>
	ResourceRegistry<T>& GetRegistry();

	void METHOD(ProcessPendingDestruction)();

protected:
	void METHOD(EnqueueDeferredFree)(GenericHandle handle);
private:
#pragma endregion

#pragma region MEMBER
public:
protected:
	ResourceRegistry<Texture>              texture_registry;
	ResourceRegistry<Buffer>               buffer_registry;
	ResourceRegistry<RenderPipelineState>  pso_registry;
	Vector<GenericHandle> deferred_free_queue;
	Vector<GenericHandle> pending_free_next;
	Vector<GenericHandle> pending_free_now;
private:
#pragma endregion
MYRENDERER_END_CLASS

// GetRegistry specializations (must be after class definition, in header for template instantiation)
template<> inline ResourceRegistry<Texture>& ResourceManager::GetRegistry<Texture>() { return texture_registry; }
template<> inline ResourceRegistry<Buffer>&  ResourceManager::GetRegistry<Buffer>()  { return buffer_registry; }
template<> inline ResourceRegistry<RenderPipelineState>& ResourceManager::GetRegistry<RenderPipelineState>() { return pso_registry; }

extern ResourceManager* g_resource_manager;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

template<typename HandleType>
auto Resolve(HandleType h) -> decltype(MXRender::RHI::g_resource_manager->Resolve(h))
{
	if (!MXRender::RHI::g_resource_manager) return decltype(MXRender::RHI::g_resource_manager->Resolve(h)){};
	return MXRender::RHI::g_resource_manager->Resolve(h);
}

template<typename HandleType>
HandleType Create(const typename MXRender::RHI::HandleTraits<HandleType>::Desc& desc, const String& name = "")
{
	if (!MXRender::RHI::g_resource_manager) return HandleType{};
	return MXRender::RHI::g_resource_manager->Create<HandleType>(desc, name);
}

template<typename HandleType>
HandleType Import(typename MXRender::RHI::HandleTraits<HandleType>::Ptr existing, const String& name = "")
{
	if (!MXRender::RHI::g_resource_manager) return HandleType{};
	return MXRender::RHI::g_resource_manager->Import<HandleType>(existing, name);
}

template<typename HandleType>
void Destroy(HandleType h)
{
	if (MXRender::RHI::g_resource_manager)
		MXRender::RHI::g_resource_manager->Destroy<HandleType>(h);
}

#endif
