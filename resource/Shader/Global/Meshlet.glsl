#ifndef MESHLET_GLSL
#define MESHLET_GLSL

// GLSL mirror of src/Runtime/Render/GPUScene/MeshletData.h.
// The C++ side carries matching static_asserts — keep both in sync.

// One cluster (32 bytes).
struct GPUCluster
{
    vec4 bounds;            // xyz = center (model space), w = radius
    uint vertexRefOffset;   // into the cluster vertex-ref list
    uint triangleOffset;    // into the cluster triangle list, in index units
    uint vertexCount;
    uint triangleCount;
};

// Per-cluster bookkeeping (16 bytes).
struct GPUClusterDraw
{
    uint meshID;
    uint lodLevel;
    uint vertexCount;
    uint triangleCount;
};

// One (cluster, object) candidate pair (8 bytes). The unit the culler tests:
// the same cluster is visible for one instance and hidden for another.
struct GPUClusterInstance
{
    uint clusterID;
    uint objectID;
};

// Set-1 binding slots reserved for the cluster path. Deliberately above the
// object path's 0..6 so a shader using both never confuses the two.
#define MESHLET_BIND_CLUSTERS       7
#define MESHLET_BIND_VERTEX_REFS    8
#define MESHLET_BIND_TRIANGLES      9
#define MESHLET_BIND_PAIRS          10
#define MESHLET_BIND_VERTICES       11
#define MESHLET_BIND_COMMANDS       12
#define MESHLET_BIND_DEBUG          13
#define MESHLET_BIND_CLUSTER_DRAWS  14

// ---- Debug views ------------------------------------------------------------
// Mirrors ENUM_CLUSTER_DEBUG in MeshletData.h. Selected through lodParams.z.

#define GPU_CLUSTER_DEBUG_OFF        0u
#define GPU_CLUSTER_DEBUG_CLUSTER_ID 1u
#define GPU_CLUSTER_DEBUG_CULL_STATE 2u
#define GPU_CLUSTER_DEBUG_LOD_LEVEL  3u
#define GPU_CLUSTER_DEBUG_MATERIAL   4u
#define GPU_CLUSTER_DEBUG_TRI_DENSITY 5u
#define GPU_CLUSTER_DEBUG_HIZ_LEVEL  6u

// Mirrors the kClusterCull* bits in MeshletData.h.
#define GPU_CLUSTER_CULL_FRUSTUM   (1u << 0)
#define GPU_CLUSTER_CULL_DISTANCE  (1u << 1)
#define GPU_CLUSTER_CULL_OCCLUSION (1u << 2)
#define GPU_CLUSTER_CULL_DRAWN     (1u << 3)

// Deterministic hash -> saturated colour, so neighbouring ids look different.
vec3 ClusterDebugHashColor(uint id)
{
    uint h = id * 2654435761u;
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return vec3(float(h & 255u), float((h >> 8) & 255u), float((h >> 16) & 255u)) / 255.0;
}

// Colour that explains WHY a cluster is or is not on screen. This is the whole
// point of the cull-state view: a missing object is otherwise indistinguishable
// from a broken matrix.
vec3 ClusterDebugCullColor(uint state)
{
    // Most specific rejection first, so the first failing test is what you see.
    if ((state & GPU_CLUSTER_CULL_FRUSTUM) == 0u)  return vec3(0.90, 0.15, 0.15);  // outside the frustum
    if ((state & GPU_CLUSTER_CULL_DISTANCE) == 0u) return vec3(0.95, 0.55, 0.10);  // past the draw distance
    if ((state & GPU_CLUSTER_CULL_OCCLUSION) == 0u) return vec3(0.20, 0.45, 0.95); // occluded
    return vec3(0.20, 0.85, 0.30);                                                 // drawn
}

// Blue (few triangles) -> red (many), so an unbalanced partition is obvious.
vec3 ClusterDebugDensityColor(uint triangle_count)
{
    const float t = clamp(float(triangle_count) / 124.0, 0.0, 1.0);
    return mix(vec3(0.15, 0.35, 0.95), vec3(0.95, 0.20, 0.15), t);
}

vec3 ClusterDebugLodColor(uint lod)
{
    if (lod == 0u) return vec3(0.20, 0.85, 0.30);
    if (lod == 1u) return vec3(0.85, 0.85, 0.25);
    if (lod == 2u) return vec3(0.95, 0.55, 0.15);
    return vec3(0.90, 0.20, 0.20);
}

#endif
