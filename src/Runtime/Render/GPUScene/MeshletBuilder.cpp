#include "Render/GPUScene/MeshletData.h"
#include <limits>

// windows.h reaches this translation unit through the RHI headers and defines
// min/max as macros, which breaks std::numeric_limits<...>::max() and would
// also swallow glm::min / glm::max. Undefine them here (see CLAUDE.md: "Window.h
// pulls in windows.h via GLFW/Vulkan — its min/max macros break std::min/max").
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)
MYRENDERER_BEGIN_NAMESPACE(GPUScene)

namespace
{
	// Maps a global vertex index to its position within the cluster currently
	// being built. A flat vector indexed by global id is used instead of a hash
	// map because the caller reuses one instance across clusters (clear() keeps
	// the allocation, and the mesh's vertex range is known to be small).
	struct LocalVertexMap
	{
		Vector<Int> local_of_global;   // -1 = not in this cluster yet

		void Reset(UInt32 global_vertex_count)
		{
			local_of_global.assign(global_vertex_count, -1);
		}

		void Clear() { local_of_global.assign(local_of_global.size(), -1); }

		Bool Contains(UInt32 global) const
		{
			return global < local_of_global.size() && local_of_global[global] >= 0;
		}

		// Returns the cluster-LOCAL index, assigning a new one if needed.
		// `cluster_base` is the offset this cluster's vertex list starts at inside
		// vertex_refs — the local index must be relative to it, not the absolute
		// position in vertex_refs, or the triangle list would address the wrong
		// vertices (and run past the cluster's own vertex count).
		UInt32 GetOrAdd(UInt32 global, Vector<UInt32>& vertex_refs, UInt32 cluster_base)
		{
			if (Contains(global))
				return static_cast<UInt32>(local_of_global[global]);

			const UInt32 local = static_cast<UInt32>(vertex_refs.size()) - cluster_base;
			vertex_refs.push_back(global);
			local_of_global[global] = static_cast<Int>(local);
			return local;
		}

		UInt32 Count() const
		{
			UInt32 n = 0;
			for (Int v : local_of_global) if (v >= 0) ++n;
			return n;
		}
	};
}

void MeshletBuilder::Build(
	CONST Vector<Tool::MeshVertex>& in_vertices,
	CONST Vector<UInt32>& in_indices,
	UInt32 in_mesh_id,
	UInt32 in_lod_level,
	Result& out_result,
	UInt32 in_max_vertices,
	UInt32 in_max_triangles)
{
	if (in_vertices.empty() || in_indices.size() < 3)
		return;

	const UInt32 triangle_total = static_cast<UInt32>(in_indices.size() / 3u);

	LocalVertexMap local_map;
	local_map.Reset(static_cast<UInt32>(in_vertices.size()));

	// Cluster being accumulated. Its refs/triangles are appended directly to the
	// result vectors, so closing a cluster only needs to record the ranges.
	UInt32 cluster_vertex_base = static_cast<UInt32>(out_result.vertex_refs.size());
	UInt32 cluster_tri_base = static_cast<UInt32>(out_result.triangles.size());
	UInt32 cluster_triangle_count = 0;

	auto close_cluster = [&]()
	{
		if (cluster_triangle_count == 0u)
			return;

		const UInt32 vertex_count = static_cast<UInt32>(out_result.vertex_refs.size()) - cluster_vertex_base;
		const UInt32 triangle_count = cluster_triangle_count;

		// Bounds from the actual vertices, in model space — the same convention
		// objects use, so the culling shader transforms it the same way.
		glm::vec3 bmin(std::numeric_limits<Float32>::max());
		glm::vec3 bmax(-std::numeric_limits<Float32>::max());
		for (UInt32 i = 0; i < vertex_count; ++i)
		{
			const UInt32 global = out_result.vertex_refs[cluster_vertex_base + i];
			const Tool::MeshVertex& v = in_vertices[global];
			const glm::vec3 p(v.position[0], v.position[1], v.position[2]);
			bmin = glm::min(bmin, p);
			bmax = glm::max(bmax, p);
		}

		const glm::vec3 center = (bmin + bmax) * 0.5f;
		const Float32 radius = glm::length(bmax - bmin) * 0.5f;

		GPUCluster cluster{};
		cluster.bounds = glm::vec4(center, radius);
		cluster.vertex_ref_offset = cluster_vertex_base;
		cluster.triangle_offset = cluster_tri_base;
		cluster.vertex_count = vertex_count;
		cluster.triangle_count = triangle_count;
		out_result.clusters.push_back(cluster);

		GPUClusterDraw draw{};
		draw.mesh_id = in_mesh_id;
		draw.lod_level = in_lod_level;
		draw.vertex_count = vertex_count;
		draw.triangle_count = triangle_count;
		out_result.cluster_draws.push_back(draw);

		// Start the next cluster where this one ended.
		cluster_vertex_base = static_cast<UInt32>(out_result.vertex_refs.size());
		cluster_tri_base = static_cast<UInt32>(out_result.triangles.size());
		cluster_triangle_count = 0;
		local_map.Clear();
	};

	for (UInt32 t = 0; t < triangle_total; ++t)
	{
		const UInt32 i0 = in_indices[t * 3u + 0u];
		const UInt32 i1 = in_indices[t * 3u + 1u];
		const UInt32 i2 = in_indices[t * 3u + 2u];

		// How many vertices would this triangle add?
		UInt32 new_vertices = 0;
		if (!local_map.Contains(i0)) ++new_vertices;
		if (!local_map.Contains(i1)) ++new_vertices;
		if (!local_map.Contains(i2)) ++new_vertices;

		const UInt32 current_vertices = local_map.Count();
		const Bool would_overflow =
			(current_vertices + new_vertices > in_max_vertices) ||
			(cluster_triangle_count + 1u > in_max_triangles);

		// A triangle is never split across clusters, so a triangle that cannot
		// fit even in an empty cluster is dropped rather than corrupting the
		// index list. With the standard 64/124 limits that cannot happen (a
		// triangle needs at most 3 vertices), but the guard keeps the invariant
		// true if the limits are lowered.
		if (would_overflow)
		{
			close_cluster();

			if (new_vertices > in_max_vertices)
				continue;
		}

		out_result.triangles.push_back(local_map.GetOrAdd(i0, out_result.vertex_refs, cluster_vertex_base));
		out_result.triangles.push_back(local_map.GetOrAdd(i1, out_result.vertex_refs, cluster_vertex_base));
		out_result.triangles.push_back(local_map.GetOrAdd(i2, out_result.vertex_refs, cluster_vertex_base));
		++cluster_triangle_count;
	}

	close_cluster();
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
