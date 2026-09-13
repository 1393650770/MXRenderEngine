#version 460
#extension GL_GOOGLE_include_directive : require

#include "../Global/GPUScene.glsl"

// Hi-Z pyramid inspector.
//
// Occlusion culling is the one of the three tests that depends entirely on data
// the CPU never sees. When it misbehaves there is no way to tell whether the
// pyramid is wrong or the test against it is — so this draws the pyramid.
//
// Depth is NOT shown raw. Under a perspective projection almost everything lands
// within a whisker of 1.0: at a 22-unit camera distance with a 200-unit far plane,
// geometry sits at ~0.9955 and empty sky is 1.0. A raw gradient of that is a flat
// wash. It is linearised first, which is why the near and far planes are
// recovered from the projection matrix below.

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

layout(std430, set = 0, binding = 0) readonly buffer SceneUniforms
{
	GPUSceneUniformsData u;
} g_scene;

layout(set = 0, binding = 1) uniform sampler2D g_hiz0;
layout(set = 0, binding = 2) uniform sampler2D g_hiz1;
layout(set = 0, binding = 3) uniform sampler2D g_hiz2;
layout(set = 0, binding = 4) uniform sampler2D g_hiz3;
layout(set = 0, binding = 5) uniform sampler2D g_hiz4;

// Recovers the clip planes from the projection matrix rather than plumbing two
// more fields through the uniform block. For a 0..1 depth projection:
//   P[2][2] = -far / (far - near)
//   P[3][2] = -far * near / (far - near)
// so P[3][2] / P[2][2] == near, and far follows from that.
void RecoverClipPlanes(out float out_near, out float out_far)
{
	const float p22 = g_scene.u.viewProj[2][2];
	const float p32 = g_scene.u.viewProj[3][2];

	// A degenerate matrix would divide by zero; clamp to something sane so the
	// view degrades to a flat colour instead of NaN.
	out_near = (abs(p22) > 1e-6) ? (p32 / p22) : 0.1;
	out_far = (abs(p22 + 1.0) > 1e-6) ? ((p22 * out_near) / (p22 + 1.0)) : 200.0;
}

float LinearizeDepth(float in_depth, float in_near, float in_far)
{
	// Maps 0..1 non-linear depth back to view-space distance, then to 0..1.
	const float view_z = (in_near * in_far) / (in_far - in_depth * (in_far - in_near));
	return clamp(view_z / in_far, 0.0, 1.0);
}

void main()
{
	// Which level to inspect comes from lodParams.w; lodParams.z says a view is on.
	const int level = int(g_scene.u.lodParams.w + 0.5);

	float raw_depth;
	if (level <= 0)      raw_depth = texture(g_hiz0, in_uv).x;
	else if (level == 1) raw_depth = texture(g_hiz1, in_uv).x;
	else if (level == 2) raw_depth = texture(g_hiz2, in_uv).x;
	else if (level == 3) raw_depth = texture(g_hiz3, in_uv).x;
	else                 raw_depth = texture(g_hiz4, in_uv).x;

	float near_plane;
	float far_plane;
	RecoverClipPlanes(near_plane, far_plane);

	const float linear = LinearizeDepth(raw_depth, near_plane, far_plane);

	// Warm = near, cold = far. Untouched texels read 1.0 (the clear value) and
	// therefore land at the far end, so empty sky is deep blue and geometry is
	// clearly separated from it — which is exactly the question being asked.
	vec3 color = mix(vec3(0.98, 0.88, 0.25), vec3(0.06, 0.10, 0.42), linear);

	out_color = vec4(color, 1.0);
}
