#if PLATFORM_GLES3

#include "RHI/GLES3/GLES3_ShaderResourceBinding.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

GLES3_ShaderResourceBinding::~GLES3_ShaderResourceBinding()
{
}

void GLES3_ShaderResourceBinding::SetResource(CONST String& name, CONST RenderResource* resource)
{
	// Phase 0 (HelloTriangle): no resources bound, no-op.
	(void)name;
	(void)resource;
}

void GLES3_ShaderResourceBinding::FlushDescriptorWrites()
{
	// No deferred writes: GL state is set immediately
}

void GLES3_ShaderResourceBinding::ApplyBindings()
{
	// Phase 0 (HelloTriangle): no bindings to apply.
	// Phase 1+: iterate m_bindings and call glUniform*/glActiveTexture/glBindTexture
}

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
