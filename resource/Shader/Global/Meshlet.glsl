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

#endif
