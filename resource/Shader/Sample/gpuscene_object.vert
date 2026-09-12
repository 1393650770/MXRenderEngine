#version 460
#extension GL_GOOGLE_include_directive : require

#include "../Global/GPUScene.glsl"

// Stage 1 vertex shader: resolve the object purely by index.
//
// The whole point of GPU Scene is here — nothing about this instance arrives
// through a descriptor. The instance index indexes visibleIDs[], which yields an
// object id, which indexes objects[]. Material and geometry are then read by
// index too. The CPU never binds anything per object.

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out flat uint v_material_id;

layout(std430, set = 0, binding = 0) readonly buffer SceneUniforms
{
	GPUSceneUniformsData u;
} g_scene;

layout(std430, set = 1, binding = 0) readonly buffer Objects    { GPUObjectData   objects[];   } g_objects;
layout(std430, set = 1, binding = 1) readonly buffer Materials  { GPUMaterialData materials[]; } g_materials;
layout(std430, set = 1, binding = 2) readonly buffer Meshes     { GPUMeshMeta     meshes[];    } g_meshes;
layout(std430, set = 1, binding = 3) readonly buffer VisibleIDs { uint            ids[];       } g_visible;

// Instance offset of the current batch inside the shared visibleIDs[] array.
//
// This exists only because the engine does not enable the `shaderDrawParameters`
// device feature, so gl_InstanceIndex restarts at 0 for every indirect command
// and DrawIndexedIndirectArgs::firstInstance is ignored. Once that feature is
// enabled (stage 4) this push constant can be deleted and all batches collapsed
// into a single DrawIndexedIndirect call.
layout(push_constant) uniform PushConstants
{
	uint batchBaseInstance;
} pc;

void main()
{
	const uint objectID = g_visible.ids[pc.batchBaseInstance + gl_InstanceIndex];
	const GPUObjectData obj = g_objects.objects[objectID];

	gl_Position = g_scene.u.viewProj * obj.model * vec4(in_pos, 1.0);
	v_normal    = mat3(obj.model) * in_normal;
	v_uv        = in_uv;
	v_material_id = obj.materialID;
}
