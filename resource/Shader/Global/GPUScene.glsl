#ifndef GPUSCENE_GLSL
#define GPUSCENE_GLSL

// GLSL mirror of src/Runtime/Render/GPUScene/GPUSceneData.h.
// The C++ side carries matching static_asserts — keep both in sync.
//
// NOTE ON INCLUDES: glslangValidator is invoked without -I, so `#include` paths
// resolve relative to the shader file. Shaders under Sample/ reach this file via
// "../Global/GPUScene.glsl". If that ever breaks, inline these structs instead.

struct GPUObjectData
{
    mat4  model;
    vec4  sphereBounds;      // xyz = center (model space), w = radius
    uint  materialID;
    uint  meshID;
    uint  flags;
    float lodBias;
};                            // 96 bytes

struct GPUMaterialData
{
    uint baseColorIdx;       // bindless texture index (set 2)
    uint normalIdx;
    uint aormIdx;
    uint flags;
    vec4 baseColorFactor;
    float metallic;
    float roughness;
    float alphaCutoff;
    uint shaderID;
};                            // 48 bytes

struct GPUMeshMeta
{
    uint indexCount;
    uint firstIndex;
    int  vertexOffset;
    uint lodCount;
    vec4 bounds;             // model-space bounds sphere
    uint lodOffsets[4];
};                            // 48 bytes

struct GPUSceneUniformsData
{
    mat4 viewProj;
    mat4 view;
    vec4 cameraPosition;
    vec4 lightDir;
    vec4 frustumPlanes[6];
    vec4 hizAndDepth;        // xy = hiz size, z = znear, w = zfar
    uvec4 counts;            // x = objectCount, y = frameIndex, z = culling, w = occlusion
};                           // 288 bytes

// Mirrors RHI::DrawIndexedIndirectArgs (RenderCommandList.h) byte for byte.
struct DrawIndexedIndirectArgs
{
    uint indexCount;
    uint instanceCount;      // written by the culling shader
    uint firstIndex;
    int  vertexOffset;
    uint firstInstance;      // unused: shaderDrawParameters is disabled
};

// Mirrors GPUScene::GPUBatch (GPUSceneData.h).
struct GPUBatchData
{
    uint meshID;
    uint baseInstance;       // start of this batch's slice in visibleIDs[]
    uint instanceCount;
    uint maxInstanceCount;
};

// One entry per object: the culling shader's input stream.
// Built once at registration; never changes unless objects are added/removed.
struct GPUInstanceData
{
    uint objectID;
    uint batchID;
};

// Bindless set is reserved at set 2 — never place scene data there.
#define GPU_SCENE_TEXTURE_SET 2
#define GPU_SCENE_OBJECT_SET  1
#define GPU_SCENE_UNIFORM_SET 0

#endif
