// Sample 16-GpuDriven — GPU Scene, stage 1: data layer + fully indirect draw.
//
// What this sample proves:
//   * One SRB bound once at init; zero descriptor bindings between draws.
//   * Every object's parameters arrive as array reads (objects[] / materials[]),
//     never as a per-object descriptor.
//   * Geometry from separate meshes lives in one merged vertex/index pool, so a
//     single indirect command can address any of them.
//
// Headless unit test: GPUDRIVEN_SELFTEST=1 runs the CPU-side checks (index
// rebase, batch partitioning, visibleIDs layout, indirect args) with no device,
// no window and no GPU, then exits with 0/1.
#include "Application/CameraController.h"   // pulls SceneView.h (glm 0..1 depth) FIRST
#include "Application/SampleApp.h"
#include "Application/Window.h"
#include "Render/GPUScene/GPUSceneData.h"
#include "Render/GPUScene/MeshPool.h"
#include "Render/GPUScene/GPUScene.h"
#include "Render/GPUScene/MeshletScene.h"
#include "Tool/MeshLoader.h"
#include "Tool/ShaderLibrary.h"
#include "Tool/BufferUtils.h"
#include "Render/Core/RenderGraph.h"
#include "Render/Core/RenderGraphPass.h"
#include "RHI/RenderRHI.h"
#include "RHI/RenderResource.h"
#include "RHI/RenderCommandList.h"
#include "RHI/RenderPipelineState.h"
#include "RHI/RenderBuffer.h"
#include <iostream>
#include <cstdlib>
#include <cstring>

using namespace MXRender;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;
using namespace MXRender::Render::GPUScene;

// Number of Hi-Z levels. Level 0 is full resolution and each level halves, so 5
// levels covers a 16:1 footprint range — enough that an object's screen extent
// maps to roughly one texel at some level.
static constexpr UInt32 kHizLevels = 5;
// Two pyramids, alternating per frame. Frame N culls against frame N-1's depth,
// which is what lets the depth prepass go away: the main pass publishes depth
// as a second render target instead of a separate geometry pass producing it.
static constexpr UInt32 kHizBuffers = 2;

struct GpuDrivenPassData : public RenderGraphPassDataBase
{
	RenderPipelineState*  pso = nullptr;
	ShaderResourceBinding* srb = nullptr;
	// Double-buffered draw passes need one pipeline per Hi-Z buffer: the render
	// pass is baked into the pipeline (the PSO cache keys on it), so a per-frame
	// render target means a pipeline per target.
	RenderPipelineState*  pso_by_parity[2] = { nullptr, nullptr };
	// Extra SRBs for passes that need more than one binding set (the Hi-Z build
	// binds a different source/destination pair per level).
	Vector<ShaderResourceBinding*> extra_srbs;
	~GpuDrivenPassData() override = default;
	void Release() override
	{
		delete srb;
		srb = nullptr;
		// pso_by_parity entries are owned by the RHI's pipeline cache.
		for (ShaderResourceBinding* s : extra_srbs) delete s;
		extra_srbs.clear();
	}
};

// Gribb-Hartmann plane extraction, adjusted for the engine's 0..1 depth range
// (Vulkan convention: 0 <= z <= w in clip space, so the near plane is row2
// rather than row3 + row2).
static void ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 out[6])
{
	const glm::vec4 row0(vp[0][0], vp[1][0], vp[2][0], vp[3][0]);
	const glm::vec4 row1(vp[0][1], vp[1][1], vp[2][1], vp[3][1]);
	const glm::vec4 row2(vp[0][2], vp[1][2], vp[2][2], vp[3][2]);
	const glm::vec4 row3(vp[0][3], vp[1][3], vp[2][3], vp[3][3]);

	out[0] = row3 + row0;   // left
	out[1] = row3 - row0;   // right
	out[2] = row3 + row1;   // bottom
	out[3] = row3 - row1;   // top
	out[4] = row2;          // near: z >= 0
	out[5] = row3 - row2;   // far:  z <= w

	for (Int i = 0; i < 6; ++i)
	{
		const Float32 len = glm::length(glm::vec3(out[i]));
		if (len > 1e-6f) out[i] /= len;
	}
}

// Derives a coarse level from a loaded mesh by keeping a subset of the
// triangles.
//
// This is NOT a simplifier — it drops faces rather than decimating, so the
// result is visibly incomplete. It exists so the LOD path is driven by real
// geometry instead of being dead code: with two registered levels the culling
// shader really does pick between them and really does write into different
// batch slices. A shipping pipeline would take artist-generated levels (or a
// mesh simplifier) here; the engine side is identical either way.
Tool::MeshDataPayload MakeCoarseLOD(const Tool::MeshDataPayload& full)
{
	Tool::MeshDataPayload coarse = full;
	const UInt32 triangle_count = static_cast<UInt32>(full.indices.size() / 3u);
	const UInt32 keep = triangle_count / 3u;                 // a third of the triangles
	coarse.indices.resize(keep * 3u);

	if (!coarse.sub_meshes.empty())
		coarse.sub_meshes[0].index_count = static_cast<UInt32>(coarse.indices.size());
	return coarse;
}

class GpuDrivenApp : public Application::SampleApp
{
public:
	GpuDrivenApp() = default;
	~GpuDrivenApp() override = default;

	void OnInitScene() override final;
	void OnShutdownScene() override final;
	void OnUpdate(float dt) override final;

protected:
	// Records the indirect draws for whichever pass is executing.
	void RecordDraws(RHI::CommandList* in_cmd) const;

	MeshPool  mesh_pool;
	GPUSceneManager  gpu_scene;
	MeshletScene     meshlet_scene;
	Vector<GPUObjectHandle> object_handles;   // stable across slot recycling
	Vector<UInt32> group_heads;               // usable LOD-group LOD0 ids
	Bool use_meshlet = false;
	UInt32 hiz_parity = 0;   // which Hi-Z buffer the next draw writes
	UInt32 debug_mode = 0;   // ENUM_CLUSTER_DEBUG; 0 = normal shading
	Bool   debug_cycling = false;
	Float32 debug_timer = 0.0f;
	Render::SceneView scene_view;
	Application::OrbitCameraController camera;
	RHI::Texture* hiz_textures[kHizBuffers][kHizLevels] = {};   // [buffer][level]
	UInt32 frame_index = 0;
	Float32 elapsed = 0.0f;
	Bool   culling_enabled = true;
	Bool   occlusion_enabled = false;
	Bool   has_pruned = false;
	Float32 draw_distance = 1000.0f;
};

// Shared by the depth prepass and the main pass: same geometry, same index
// chain, so the two paths can never disagree about what is being drawn.
void GpuDrivenApp::RecordDraws(RHI::CommandList* in_cmd) const
{
	const UInt32 stride = static_cast<UInt32>(sizeof(DrawIndexedIndirectArgs));
	const Vector<GPUBatch>& batches = gpu_scene.GetBatches();

	if (gpu_scene.GetUseFirstInstance())
	{
		// shaderDrawParameters is available: gl_InstanceIndex starts at each
		// command's firstInstance, so the whole scene is ONE indirect command
		// and there is zero per-draw state.
		in_cmd->DrawIndexedIndirect(gpu_scene.GetDrawCommandBuffer(), 0,
			static_cast<UInt32>(batches.size()), stride);
		return;
	}

	// Fallback: no firstInstance support, so one command per batch and the
	// slice offset rides in a push constant.
	for (UInt32 b = 0; b < batches.size(); ++b)
	{
		const UInt32 base = batches[b].base_instance;
		in_cmd->SetPushConstants(0, sizeof(UInt32), &base);
		in_cmd->DrawIndexedIndirect(gpu_scene.GetDrawCommandBuffer(), b * stride, 1);
	}
}

void GpuDrivenApp::OnInitScene()
{
	// ---- 1. merge geometry into one pool -------------------------------------
	// Two LOD groups so the level selection is actually exercised:
	//   group 0 = two levels (full + coarse), group 1 = a single level.
	// A group is addressed by its LOD0 id, and the levels live contiguously
	// after it — which is what lets the cull shader reach a level with
	// `groupBase + lod`.
	Tool::MeshDataPayload payload;
	const Bool loaded = Tool::MeshLoader::LoadMeshData("Mesh/cube.obj", payload);
	ENSURE(loaded, "GpuDriven: failed to load Mesh/cube.obj");
	if (!loaded) return;

	Vector<Tool::MeshDataPayload> lod_levels;
	lod_levels.push_back(payload);
	lod_levels.push_back(MakeCoarseLOD(payload));

	const Vector<UInt32> lod_ids = mesh_pool.AddMeshLODs(lod_levels);
	const Vector<UInt32> single_ids = mesh_pool.AddMesh(payload);
	ENSURE(!lod_ids.empty() && !single_ids.empty(), "GpuDriven: mesh registration failed");

	group_heads.push_back(lod_ids[0]);       // 2 levels
	group_heads.push_back(single_ids[0]);    // 1 level

	ENSURE(mesh_pool.Upload(), "GpuDriven: failed to upload merged mesh pool");
	std::cout << "[GpuDriven] drawable units=" << mesh_pool.GetMeshCount()
		<< " lod groups=" << mesh_pool.GetLODGroupHeads().size()
		<< " verts=" << mesh_pool.GetVertexCount()
		<< " indices=" << mesh_pool.GetIndexCount() << std::endl;

	// ---- 2. scene data --------------------------------------------------------
	GPUSceneManager::Config cfg{};
	gpu_scene.Initialize(cfg, mesh_pool);

	// GPUDRIVEN_FIRST_INSTANCE=1 switches to the single-command path. It needs
	// the shaderDrawParameters device feature (requested at device creation when
	// the GPU reports it); without it firstInstance is ignored and every batch
	// would resolve its instances against the wrong slice.
	if (std::getenv("GPUDRIVEN_FIRST_INSTANCE") != nullptr)
	{
		gpu_scene.SetUseFirstInstance(true);
		std::cout << "[GpuDriven] firstInstance path: one indirect command for the scene" << std::endl;
	}
	if (std::getenv("GPUDRIVEN_OCCLUSION") != nullptr)
	{
		occlusion_enabled = true;
		std::cout << "[GpuDriven] occlusion culling on" << std::endl;
	}
	if (const char* dbg = std::getenv("GPUDRIVEN_DEBUG"))
	{
		if (std::strcmp(dbg, "cycle") == 0)
		{
			debug_cycling = true;
			debug_mode = 1;   // start at the first real view
		}
		else
		{
			debug_mode = static_cast<UInt32>(std::atoi(dbg));
		}
		if (debug_mode != 0u && std::getenv("GPUDRIVEN_MESHLET") == nullptr)
		{
			// The views describe cluster culling; without the meshlet path there
			// is nothing to describe.
			std::cout << "[GpuDriven] cluster debug views need GPUDRIVEN_MESHLET=1 — ignoring\n";
			debug_mode = 0u;
			debug_cycling = false;
		}
		else
		{
			std::cout << "[GpuDriven] debug view " << debug_mode
				<< (debug_cycling ? " (cycling every 4s)" : "") << std::endl;
		}
	}

	if (std::getenv("GPUDRIVEN_MESHLET") != nullptr)
	{
		use_meshlet = true;
		if (!gpu_scene.GetUseFirstInstance())
		{
			// The cluster shader identifies its pair through gl_InstanceIndex.
			// Without firstInstance support every cluster would resolve to pair 0.
			std::cout << "[GpuDriven] meshlet path needs GPUDRIVEN_FIRST_INSTANCE=1 — ignoring\n";
			use_meshlet = false;
		}
	}

	// ---- Hi-Z pyramid (double buffered) ---------------------------------------
	// Level 0 is published by the main pass as a second colour target; the coarser
	// levels are produced by gpuscene_hiz.comp. Two pyramids alternate so frame N
	// culls against frame N-1's depth without either one being written and read in
	// the same frame.
	//
	// Levels are separate textures rather than one mip chain because this RHI
	// cannot bind a per-mip view, and separate bindings rather than a sampler
	// array because SetResource binds by instance name.
	//
	// Cleared to 1.0 (far plane) so an uninitialised pyramid reads as "nothing
	// here" — on the very first frames, before any depth has been published, that
	// means nothing is falsely occluded.
	{
		ENSURE(GetViewportWidth() > 0 && GetViewportHeight() > 0,
			"GpuDriven: viewport must be sized before scene init");

		const UInt32 view_w = GetViewportWidth();
		const UInt32 view_h = GetViewportHeight();

		for (UInt32 b = 0; b < kHizBuffers; ++b)
		{
			for (UInt32 k = 0; k < kHizLevels; ++k)
			{
				RHI::TextureDesc d{};
				d.width = (view_w >> k) > 0u ? (view_w >> k) : 1u;
				d.height = (view_h >> k) > 0u ? (view_h >> k) : 1u;
				d.format = ENUM_TEXTURE_FORMAT::R32F;
				d.type = ENUM_TEXTURE_TYPE::ENUM_TYPE_2D;
				d.usage = ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_SHADERRESOURCE;
				// Level 0 is rendered into by the main pass; the rest are compute
				// targets. Every level is also sampled by the culler.
				d.usage |= (k == 0u)
					? ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_COLOR_ATTACHMENT
					: ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_STORAGE;
				d.mip_level = 1;
				d.layer_count = 1;
				d.samples = 1;
				d.clear_value.color[0] = 1.0f;
				d.clear_value.color[1] = 1.0f;
				d.clear_value.color[2] = 1.0f;
				d.clear_value.color[3] = 1.0f;
				hiz_textures[b][k] = g_render_rhi->CreateTexture(d);
				ENSURE(hiz_textures[b][k] != nullptr, "GpuDriven: failed to create a Hi-Z level");
			}
		}
	}

	const glm::vec3 palette[8] = {
		{0.90f, 0.35f, 0.35f}, {0.95f, 0.60f, 0.30f}, {0.90f, 0.85f, 0.35f},
		{0.45f, 0.85f, 0.40f}, {0.35f, 0.75f, 0.85f}, {0.40f, 0.50f, 0.90f},
		{0.70f, 0.45f, 0.90f}, {0.85f, 0.85f, 0.85f},
	};
	for (UInt32 i = 0; i < 8; ++i)
	{
		GPUMaterialData mat{};
		mat.base_color_factor = glm::vec4(palette[i], 1.0f);
		mat.metallic = 0.0f;
		mat.roughness = 0.6f;
		mat.shader_id = 0;
		gpu_scene.AddMaterial(mat);
	}

	constexpr Int     grid_x = 8, grid_y = 8, grid_z = 4;
	constexpr Float32 spacing = 1.7f;
	const glm::vec3 origin = glm::vec3(
		-(grid_x - 1) * 0.5f, -(grid_y - 1) * 0.5f, -(grid_z - 1) * 0.5f) * spacing;

	for (Int z = 0; z < grid_z; ++z)
		for (Int y = 0; y < grid_y; ++y)
			for (Int x = 0; x < grid_x; ++x)
			{
				const glm::vec3 pos = origin + glm::vec3(x, y, z) * spacing;
				GPUObjectData obj{};
				obj.model = glm::translate(glm::mat4(1.0f), pos)
					* glm::scale(glm::mat4(1.0f), glm::vec3(0.45f));
				// Alternate the LOD group so both shapes get instances. The id is
				// the group's LOD0 entry, never a mid-level one.
				obj.mesh_id = group_heads[static_cast<UInt32>((x + y + z) % 2)];
				obj.material_id = static_cast<UInt32>((x + y + z) % 8);
				obj.sphere_bounds = mesh_pool.GetMeshMetas()[obj.mesh_id].bounds;
				object_handles.push_back(gpu_scene.AddObject(obj));
			}

	gpu_scene.BuildBatches(mesh_pool);
	gpu_scene.BuildDrawCommands(mesh_pool);
	gpu_scene.UploadObjects();
	gpu_scene.UploadMaterials();
	gpu_scene.UploadVisibleIDs();
	gpu_scene.UploadInstances();
	gpu_scene.UploadBatches();
	gpu_scene.UploadDrawCommands();

	// ---- meshlet clusters -----------------------------------------------------
	// Built from the instance stream, so it must follow BuildBatches.
	if (use_meshlet)
	{
		// A cube is 12 triangles, so the default 124-triangle cap would make it a
		// SINGLE cluster and the cluster pass would have nothing to reject. A
		// smaller cap forces a real split so the path is exercised. Real geometry
		// (thousands of triangles) splits naturally at the default cap.
		meshlet_scene.Build(mesh_pool, gpu_scene, kMeshletMaxVertices, 8u);
		std::cout << "[GpuDriven] meshlets: clusters=" << meshlet_scene.GetClusterCount()
			<< " pairs=" << meshlet_scene.GetClusterInstanceCount()
			<< " commands=" << meshlet_scene.GetCommandCount() << std::endl;
	}

	std::cout << "[GpuDriven] objects=" << gpu_scene.GetObjectCount()
		<< " materials=" << gpu_scene.GetMaterialCount()
		<< " batches=" << gpu_scene.GetBatchCount() << std::endl;

	// ---- 3. camera ------------------------------------------------------------
	camera.Attach(GetPlatformWindow());
	camera.distance = 22.0f;
	scene_view.SetPerspective(glm::radians(45.0f), 0.1f, 200.0f);

	// ---- pass chain -----------------------------------------------------------
	// Temporal occlusion: frame N culls against frame N-1's pyramid, which is what
	// lets the depth prepass disappear. The main pass publishes depth as a second
	// render target on its way past, so the scene is rasterised ONCE instead of
	// twice. The cost is one frame of lag in the occlusion test, which surfaces as
	// occasional over-draw rather than any visual error — the MAX reduction can
	// only under-cull.
	//
	// Order is therefore: cull (previous frame's pyramid) -> draw (writes this
	// frame's level 0) -> build the rest of this frame's pyramid.
	// Parity lives in a member and is re-read inside each execute lambda — a
	// captured value would freeze on the init-time parity and quietly turn the
	// double buffering into single buffering.

	// One helper so the cull/draw passes do not each repeat the target setup.
	auto bind_hiz_target = [this](CommandList* in_cmd)
	{
		const UInt32 cur_buffer = hiz_parity;
		if (!hiz_textures[cur_buffer][0]) return;

		in_cmd->TransitionTextureState(hiz_textures[cur_buffer][0], ENUM_RESOURCE_STATE::RenderTarget);

		Vector<Texture*> rtvs = { GetBackBuffer(), hiz_textures[cur_buffer][0] };
		Vector<ClearValue> clears;
		clears.push_back(GetBackBuffer()->GetTextureDesc().clear_value);
		clears.push_back(hiz_textures[cur_buffer][0]->GetTextureDesc().clear_value);   // 1.0 = far
		if (GetDepthStencil()) clears.push_back(GetDepthStencil()->GetTextureDesc().clear_value);
		in_cmd->SetRenderTarget(rtvs, GetDepthStencil(), clears, GetDepthStencil() != nullptr);
	};

	// Binds a pyramid's five levels to a cull shader's SRB. `buffer` selects which
	// of the two pyramids — that is the whole point of double buffering here:
	// culling reads the one the previous frame finished writing.
	auto bind_hiz_levels = [this](ShaderResourceBinding* srb, UInt32 buffer)
	{
		for (UInt32 k = 0; k < kHizLevels; ++k)
		{
			if (!hiz_textures[buffer][k]) continue;
			switch (k)
			{
			case 0: srb->SetResource("g_hiz0", hiz_textures[buffer][0]); break;
			case 1: srb->SetResource("g_hiz1", hiz_textures[buffer][1]); break;
			case 2: srb->SetResource("g_hiz2", hiz_textures[buffer][2]); break;
			case 3: srb->SetResource("g_hiz3", hiz_textures[buffer][3]); break;
			default: srb->SetResource("g_hiz4", hiz_textures[buffer][4]); break;
			}
		}
	};

	if (!use_meshlet)
	{
		// ---- cull: reads the PREVIOUS frame's pyramid -------------------------
		auto* cull_pass = graph.AddRenderPass<GpuDrivenPassData>("GpuSceneCull", &graph,
			RHIGetImmediateCommandList(),
			[&](GpuDrivenPassData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd)
			{
				Shader* cs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Compute,
					"Shader/gpuscene_cull.comp.spv");
				data.pso = Tool::ShaderLibrary::CreateComputePSO(cs);

				// One SRB per pyramid: the binding names are identical, only the
				// textures differ, and which one is used is a per-frame choice.
				for (UInt32 b = 0; b < kHizBuffers; ++b)
				{
					ShaderResourceBinding* srb = nullptr;
					data.pso->CreateShaderResourceBinding(srb, false);
					srb->SetResource("g_scene", gpu_scene.GetUniformBuffer());
					srb->SetResource("g_objects", gpu_scene.GetObjectBuffer());
					srb->SetResource("g_meshes", gpu_scene.GetMeshBuffer());
					srb->SetResource("g_visible", gpu_scene.GetVisibleIDBuffer());
					srb->SetResource("g_candidates", gpu_scene.GetInstanceBuffer());
					srb->SetResource("g_commands", gpu_scene.GetDrawCommandBuffer());
					srb->SetResource("g_batches", gpu_scene.GetBatchBuffer());
					bind_hiz_levels(srb, b);
					srb->FlushDescriptorWrites();
					data.extra_srbs.push_back(srb);
				}
				delete cs;
			},
			[this](CONST GpuDrivenPassData& data, CommandList* in_cmd)
			{
				const UInt32 prev_buffer = (hiz_parity + 1u) % kHizBuffers;
				const UInt32 count = gpu_scene.GetActiveObjectCount();
				if (count == 0) return;
				if (prev_buffer >= data.extra_srbs.size()) return;

				// The pyramid this pass reads was written last frame; make sure the
				// first few frames (before any build ran) have it in the sampled
				// state rather than leaving it as a colour target.
				for (UInt32 k = 0; k < kHizLevels; ++k)
				{
					if (hiz_textures[prev_buffer][k])
						in_cmd->TransitionTextureState(hiz_textures[prev_buffer][k],
							ENUM_RESOURCE_STATE::ShaderResource);
				}

				in_cmd->SetComputePipeline(data.pso);
				in_cmd->SetShaderResourceBinding(data.extra_srbs[prev_buffer]);
				in_cmd->Dispatch((count + 63u) / 64u, 1, 1);

				in_cmd->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess,
					ENUM_RESOURCE_STATE::IndirectArgument);
				in_cmd->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess,
					ENUM_RESOURCE_STATE::ShaderResource);
			});
		cull_pass->SetIsCullable(false);
		cull_pass->SetShaderPath("Shader/gpuscene_cull");

		// ---- draw: writes colour AND this frame's pyramid level 0 -------------
		auto* draw_pass = graph.AddRenderPass<GpuDrivenPassData>("GpuDrivenDraw", &graph,
			RHIGetImmediateCommandList(),
			[&](GpuDrivenPassData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd)
			{
				builder.Write(GetBackBufferResource());
				if (GetDepthStencilResource()) builder.Write(GetDepthStencilResource());

				Shader* vs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Vertex,
					"Shader/gpuscene_object.vert.spv");
				// The MRT variant: identical shading, plus a depth output.
				Shader* ps = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Pixel,
					"Shader/gpuscene_object_mrt.frag.spv");

				for (UInt32 b = 0; b < kHizBuffers; ++b)
				{
					RenderGraphiPipelineStateDesc pd{};
					pd.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
					pd.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps;
					pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
					pd.vertex_input_layout = Tool::MeshDataPayload::GetVertexInputLayout();
					// Two colour targets: the back buffer and this buffer's Hi-Z level 0.
					pd.render_targets = { GetBackBuffer(), hiz_textures[b][0] };
					pd.depth_stencil_view = GetDepthStencil();
					pd.raster_state.sample_count = 1;
					pd.raster_state.cull_mode = ENUM_RASTER_CULLMODE::Back;
					pd.depth_stencil_state.depth_test_enable = true;
					pd.depth_stencil_state.depth_write_enable = true;
					pd.depth_stencil_state.depth_func = ENUM_STENCIL_FUNCTION::ENUM_LESS;
					pd.blend_state.render_targets.resize(2);
					data.pso_by_parity[b] = g_render_rhi->CreateRenderPipelineState(pd);

					ShaderResourceBinding* srb = nullptr;
					data.pso_by_parity[b]->CreateShaderResourceBinding(srb, false);
					srb->SetResource("g_scene", gpu_scene.GetUniformBuffer());
					srb->SetResource("g_objects", gpu_scene.GetObjectBuffer());
					srb->SetResource("g_visible", gpu_scene.GetVisibleIDBuffer());
					srb->FlushDescriptorWrites();
					data.extra_srbs.push_back(srb);
				}

				delete vs;
				delete ps;
			},
			[this, bind_hiz_target](CONST GpuDrivenPassData& data, CommandList* in_cmd)
			{
				const UInt32 cur_buffer = hiz_parity;
				if (cur_buffer >= kHizBuffers || !data.pso_by_parity[cur_buffer]) return;

				bind_hiz_target(in_cmd);
				in_cmd->SetGraphicsPipeline(data.pso_by_parity[cur_buffer]);
				in_cmd->SetShaderResourceBinding(data.extra_srbs[cur_buffer]);
				in_cmd->SetVertexBuffer(mesh_pool.GetVertexBuffer(), 0, MeshPool::GetVertexStride(), 0);
				in_cmd->SetIndexBuffer(mesh_pool.GetIndexBuffer(), 0, true);
				RecordDraws(in_cmd);
			});
		draw_pass->SetIsCullable(false);
		draw_pass->SetShaderPath("Shader/gpuscene_object_mrt");
	}
	else
	{
		// ---- meshlet cull: also reads the previous frame's pyramid -----------
		auto* mcull = graph.AddRenderPass<GpuDrivenPassData>("GpuSceneMeshletCull", &graph,
			RHIGetImmediateCommandList(),
			[&](GpuDrivenPassData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd)
			{
				Shader* cs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Compute,
					"Shader/gpuscene_meshlet_cull.comp.spv");
				data.pso = Tool::ShaderLibrary::CreateComputePSO(cs);

				for (UInt32 b = 0; b < kHizBuffers; ++b)
				{
					ShaderResourceBinding* srb = nullptr;
					data.pso->CreateShaderResourceBinding(srb, false);
					srb->SetResource("g_scene", gpu_scene.GetUniformBuffer());
					srb->SetResource("g_objects", gpu_scene.GetObjectBuffer());
					srb->SetResource("g_clusters", meshlet_scene.GetClusterBuffer());
					srb->SetResource("g_cluster_instances", meshlet_scene.GetClusterInstanceBuffer());
					srb->SetResource("g_cluster_commands", meshlet_scene.GetCommandBuffer());
					srb->SetResource("g_cluster_debug", meshlet_scene.GetClusterDebugBuffer());
					bind_hiz_levels(srb, b);
					srb->FlushDescriptorWrites();
					data.extra_srbs.push_back(srb);
				}
				delete cs;
			},
			[this](CONST GpuDrivenPassData& data, CommandList* in_cmd)
			{
				const UInt32 prev_buffer = (hiz_parity + 1u) % kHizBuffers;
				const UInt32 count = meshlet_scene.GetCommandCount();
				if (count == 0) return;
				if (prev_buffer >= data.extra_srbs.size()) return;

				for (UInt32 k = 0; k < kHizLevels; ++k)
				{
					if (hiz_textures[prev_buffer][k])
						in_cmd->TransitionTextureState(hiz_textures[prev_buffer][k],
							ENUM_RESOURCE_STATE::ShaderResource);
				}

				in_cmd->SetComputePipeline(data.pso);
				in_cmd->SetShaderResourceBinding(data.extra_srbs[prev_buffer]);
				in_cmd->Dispatch((count + 63u) / 64u, 1, 1);

				in_cmd->ResourceBarrier(ENUM_RESOURCE_STATE::UnorderedAccess,
					ENUM_RESOURCE_STATE::IndirectArgument);
			});
		mcull->SetIsCullable(false);
		mcull->SetShaderPath("Shader/gpuscene_meshlet_cull");

		// ---- meshlet draw: vertex pulling, writes pyramid level 0 too ---------
		auto* mdraw = graph.AddRenderPass<GpuDrivenPassData>("GpuSceneMeshletDraw", &graph,
			RHIGetImmediateCommandList(),
			[&](GpuDrivenPassData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd)
			{
				builder.Write(GetBackBufferResource());
				if (GetDepthStencilResource()) builder.Write(GetDepthStencilResource());

				Shader* vs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Vertex,
					"Shader/gpuscene_meshlet.vert.spv");
				// The meshlet pixel shader carries the debug views (the object path
				// has its own MRT shader without them).
				Shader* ps = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Pixel,
					"Shader/gpuscene_meshlet.frag.spv");

				for (UInt32 b = 0; b < kHizBuffers; ++b)
				{
					RenderGraphiPipelineStateDesc pd{};
					pd.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
					pd.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps;
					pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
					// No vertex_input_layout: every attribute is pulled from storage.
					pd.render_targets = { GetBackBuffer(), hiz_textures[b][0] };
					pd.depth_stencil_view = GetDepthStencil();
					pd.raster_state.sample_count = 1;
					pd.raster_state.cull_mode = ENUM_RASTER_CULLMODE::Back;
					pd.depth_stencil_state.depth_test_enable = true;
					pd.depth_stencil_state.depth_write_enable = true;
					pd.depth_stencil_state.depth_func = ENUM_STENCIL_FUNCTION::ENUM_LESS;
					pd.blend_state.render_targets.resize(2);
					data.pso_by_parity[b] = g_render_rhi->CreateRenderPipelineState(pd);

					ShaderResourceBinding* srb = nullptr;
					data.pso_by_parity[b]->CreateShaderResourceBinding(srb, false);
					srb->SetResource("g_scene", gpu_scene.GetUniformBuffer());
					srb->SetResource("g_objects", gpu_scene.GetObjectBuffer());
					srb->SetResource("g_clusters", meshlet_scene.GetClusterBuffer());
					srb->SetResource("g_cluster_refs", meshlet_scene.GetVertexRefBuffer());
					srb->SetResource("g_cluster_tris", meshlet_scene.GetTriangleBuffer());
					srb->SetResource("g_cluster_instances", meshlet_scene.GetClusterInstanceBuffer());
					srb->SetResource("g_vertex_data", mesh_pool.GetVertexBuffer());
					srb->SetResource("g_cluster_draws", meshlet_scene.GetClusterDrawBuffer());
					srb->SetResource("g_cluster_debug", meshlet_scene.GetClusterDebugBuffer());
					srb->FlushDescriptorWrites();
					data.extra_srbs.push_back(srb);
				}

				delete vs;
				delete ps;
			},
			[this, bind_hiz_target](CONST GpuDrivenPassData& data, CommandList* in_cmd)
			{
				const UInt32 count = meshlet_scene.GetCommandCount();
				const UInt32 cur_buffer = hiz_parity;
				if (count == 0) return;
				if (cur_buffer >= kHizBuffers || !data.pso_by_parity[cur_buffer]) return;

				bind_hiz_target(in_cmd);
				in_cmd->SetGraphicsPipeline(data.pso_by_parity[cur_buffer]);
				in_cmd->SetShaderResourceBinding(data.extra_srbs[cur_buffer]);
				in_cmd->DrawIndexedIndirect(meshlet_scene.GetCommandBuffer(), 0, count);
			});
		mdraw->SetIsCullable(false);
		mdraw->SetShaderPath("Shader/gpuscene_meshlet");
	}

	// ---- build the rest of THIS frame's pyramid --------------------------------
	// Runs after the draw, because level 0 is one of the draw's colour targets.
	// The result is consumed by the NEXT frame's cull, which is the other half of
	// the double buffering.
	auto* hiz_pass = graph.AddRenderPass<GpuDrivenPassData>("GpuSceneHiz", &graph,
		RHIGetImmediateCommandList(),
		[&](GpuDrivenPassData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd)
		{
			Shader* cs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Compute,
				"Shader/gpuscene_hiz.comp.spv");
			data.pso = Tool::ShaderLibrary::CreateComputePSO(cs);

			// Every level pair of every buffer: four per buffer, so eight SRBs.
			for (UInt32 b = 0; b < kHizBuffers; ++b)
			{
				for (UInt32 k = 0; k + 1u < kHizLevels; ++k)
				{
					ShaderResourceBinding* srb = nullptr;
					data.pso->CreateShaderResourceBinding(srb, false);
					srb->SetResource("g_src", hiz_textures[b][k]);
					srb->SetResource("g_dst", hiz_textures[b][k + 1u]);
					srb->FlushDescriptorWrites();
					data.extra_srbs.push_back(srb);
				}
			}
			delete cs;
		},
		[this](CONST GpuDrivenPassData& data, CommandList* in_cmd)
		{
			const UInt32 cur_buffer = hiz_parity;
			if (!hiz_textures[cur_buffer][0]) return;

			// Level 0 was a colour target; the reducer samples it.
			in_cmd->TransitionTextureState(hiz_textures[cur_buffer][0],
				ENUM_RESOURCE_STATE::ShaderResource);

			const UInt32 view_w = GetViewportWidth();
			const UInt32 view_h = GetViewportHeight();

			// SRBs are laid out buffer-major: [0..kHizLevels-2] is buffer 0.
			const UInt32 base = cur_buffer * (kHizLevels - 1u);

			for (UInt32 k = 0; k + 1u < kHizLevels; ++k)
			{
				const UInt32 sw = (view_w >> k) > 0u ? (view_w >> k) : 1u;
				const UInt32 sh = (view_h >> k) > 0u ? (view_h >> k) : 1u;

				in_cmd->SetComputePipeline(data.pso);
				in_cmd->SetShaderResourceBinding(data.extra_srbs[base + k]);
				in_cmd->Dispatch((sw + 7u) / 8u, (sh + 7u) / 8u, 1);

				// Written level becomes a shader resource for the next dispatch and
				// for next frame's cull. Retained texture, so this is manual.
				in_cmd->TransitionTextureState(hiz_textures[cur_buffer][k + 1u],
					ENUM_RESOURCE_STATE::ShaderResource);
			}
		});
	hiz_pass->SetIsCullable(false);
	hiz_pass->SetShaderPath("Shader/gpuscene_hiz");
}

void GpuDrivenApp::OnShutdownScene()
{
	// Release before RHIShutdown — member destruction runs too late.
	gpu_scene.Shutdown();
	meshlet_scene.Shutdown();
	mesh_pool.Release();
	for (UInt32 b = 0; b < kHizBuffers; ++b)
	{
		for (UInt32 k = 0; k < kHizLevels; ++k)
		{
			delete hiz_textures[b][k];
			hiz_textures[b][k] = nullptr;
		}
	}
}

void GpuDrivenApp::OnUpdate(float dt)
{
	camera.Update(dt, scene_view);
	elapsed += dt;

	GPUSceneUniformsData& u = gpu_scene.GetUniforms();
	u.view_proj = scene_view.GetViewProjectionMatrix();
	u.view = scene_view.GetViewMatrix();
	u.camera_position = glm::vec4(scene_view.GetViewLocation(), 1.0f);
	u.light_dir = glm::vec4(glm::normalize(glm::vec3(0.4f, 0.8f, 0.45f)), 0.0f);
	ExtractFrustumPlanes(u.view_proj, u.frustum_planes);
	hiz_parity = frame_index % kHizBuffers;
	u.counts.x = gpu_scene.GetActiveObjectCount();
	u.counts.y = frame_index++;
	u.counts.z = culling_enabled ? 1u : 0u;
	u.counts.w = occlusion_enabled ? 1u : 0u;
	// xy = depth target size for the occlusion sample grid,
	// z  = znear, w = max draw distance (0 disables the distance cull).
	// The grid spans roughly 34 units, so the default only trims the far edge;
	// the runtime shrink below makes the effect obvious.
	u.hiz_and_depth = glm::vec4(static_cast<Float32>(GetViewportWidth()),
		static_cast<Float32>(GetViewportHeight()), 0.1f, draw_distance);
	// Cycling lets every view be inspected without an input system.
	if (debug_cycling)
	{
		debug_timer += dt;
		if (debug_timer >= 4.0f)
		{
			debug_timer = 0.0f;
			debug_mode = (debug_mode % 5u) + 1u;   // 1..5, skipping Off
			std::cout << "[GpuDriven] debug view " << debug_mode << std::endl;
		}
	}

	// LOD bands are geometric: level 1 starts past lodBase, level 2 past
	// lodBase * lodStep, and so on. The grid spans roughly 14 units around the
	// orbit target and the camera sits ~22 away, so base 20 splits it roughly in
	// half — enough to see the coarse level take over in the distance.
	// z carries the debug view selection (the slot was free, so nothing grew).
	u.lod_params = glm::vec4(20.0f, 2.5f, static_cast<Float32>(debug_mode), 0.0f);
	gpu_scene.UploadUniforms();

	// Clear instanceCount so the culling shader can accumulate it again.
	// Geometry fields (indexCount/firstIndex) are untouched by this.
	gpu_scene.ResetDrawCommands();
	if (use_meshlet)
		meshlet_scene.ResetClusterCommands();

	// Stage 4: remove a slice of the scene at runtime, then immediately refill
	// the freed slots — this is what exercises recycling.
	if (!has_pruned && elapsed > 3.0f)
	{
		has_pruned = true;
		UInt32 removed = 0;
		for (GPUObjectHandle handle : object_handles)
		{
			const GPUObjectData* obj = gpu_scene.GetObjectByHandle(handle);
			if (obj && obj->material_id == 0u)
			{
				gpu_scene.RemoveObject(handle);
				++removed;
			}
		}

		// Recycle half the freed slots with fresh objects: the slot count must
		// NOT grow if the free list is working.
		const UInt32 slots_before = gpu_scene.GetSlotCount();
		const UInt32 refill = removed / 2u;
		for (UInt32 i = 0; i < refill; ++i)
		{
			GPUObjectData fresh{};
			fresh.mesh_id = group_heads[i % 2u];
			fresh.material_id = 7u;                       // the near-white material
			fresh.model = glm::translate(glm::mat4(1.0f), glm::vec3(
				(static_cast<Float32>(i % 8) - 3.5f) * 1.7f,
				6.0f,
				(static_cast<Float32>(i / 8) - 3.5f) * 1.7f))
				* glm::scale(glm::mat4(1.0f), glm::vec3(0.45f));
			fresh.sphere_bounds = mesh_pool.GetMeshMetas()[fresh.mesh_id].bounds;
			object_handles.push_back(gpu_scene.AddObject(fresh));
		}

		gpu_scene.BuildBatches(mesh_pool);
		gpu_scene.BuildDrawCommands(mesh_pool);
		gpu_scene.UploadVisibleIDs();
		gpu_scene.UploadInstances();
		gpu_scene.UploadBatches();
		gpu_scene.UploadObjects();
		std::cout << "[GpuDriven] pruned " << removed << ", refilled " << refill
			<< " -> active=" << gpu_scene.GetActiveObjectCount()
			<< " slots=" << slots_before << "->" << gpu_scene.GetSlotCount()
			<< " free=" << gpu_scene.GetFreeSlotCount()
			<< " batches=" << gpu_scene.GetBatchCount() << std::endl;
	}

	// Tighten the draw distance afterwards so the distance cull visibly kicks
	// in — the grid spans ~34 units, so objects at the far edge start dropping.
	if (has_pruned && draw_distance > 34.0f)
		draw_distance = glm::max(34.0f, draw_distance - dt * 15.0f);

	// Slow spin. Only the 96-byte object record is rewritten — no descriptor
	// touch, which is the entire point. Iterating handles rather than slots
	// keeps this correct after slots start being recycled.
	for (GPUObjectHandle handle : object_handles)
	{
		if (!gpu_scene.IsObjectAlive(handle)) continue;
		GPUObjectData& obj = gpu_scene.GetObject(handle.index);
		const glm::vec3 pos = glm::vec3(obj.model[3]);
		obj.model = glm::translate(glm::mat4(1.0f), pos)
			* glm::rotate(glm::mat4(1.0f), elapsed * 0.35f + handle.index * 0.01f, glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(0.45f));
		gpu_scene.MarkObjectDirty(handle.index);
	}
	gpu_scene.UploadObjects();
}

// ---------------------------------------------------------------------------
// Headless unit test — no device, no window, no GPU.
// ---------------------------------------------------------------------------
namespace
{
Int g_failures = 0;

void Check(Bool condition, const char* name)
{
	if (!condition) ++g_failures;
	std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << std::endl;
}

Tool::MeshDataPayload MakeTrianglePayload()
{
	Tool::MeshDataPayload p;
	p.vertices.resize(3);
	for (UInt32 i = 0; i < 3; ++i)
	{
		p.vertices[i].position[0] = static_cast<Float32>(i);
		p.vertices[i].position[1] = 0.0f;
		p.vertices[i].position[2] = 0.0f;
		p.vertices[i].normal[0] = 0.0f;
		p.vertices[i].normal[1] = 1.0f;
		p.vertices[i].normal[2] = 0.0f;
		p.vertices[i].uv[0] = 0.0f;
		p.vertices[i].uv[1] = 0.0f;
	}
	p.indices = { 0u, 1u, 2u };

	Tool::MeshDataPayload::SubMesh sub{};
	sub.index_offset = 0;
	sub.index_count = 3;
	p.sub_meshes.push_back(sub);

	p.bounds_min[0] = 0.0f; p.bounds_min[1] = 0.0f; p.bounds_min[2] = 0.0f;
	p.bounds_max[0] = 2.0f; p.bounds_max[1] = 0.0f; p.bounds_max[2] = 0.0f;
	return p;
}
}

static Int RunSelfTest()
{
	std::cout << "=== GPU Scene self-test (headless) ===" << std::endl;

	// --- layout contract ---
	Check(sizeof(GPUObjectData) == 96, "GPUObjectData is 96 bytes");
	Check(sizeof(GPUMaterialData) == 48, "GPUMaterialData is 48 bytes");
	Check(sizeof(GPUMeshMeta) == 48, "GPUMeshMeta is 48 bytes");
	Check(sizeof(GPUSceneUniformsData) == 288, "GPUSceneUniformsData is 288 bytes");

	// --- mesh pool: merging and index rebase ---
	MeshPool pool;
	const Vector<UInt32> ids_a = pool.AddMesh(MakeTrianglePayload());
	const Vector<UInt32> ids_b = pool.AddMesh(MakeTrianglePayload());

	Check(ids_a.size() == 1 && ids_b.size() == 1, "single-material mesh yields one drawable unit");
	Check(ids_a[0] == 0u && ids_b[0] == 1u, "drawable unit ids increase");
	Check(pool.GetVertexCount() == 6, "vertices merged into one pool");
	Check(pool.GetIndexCount() == 6, "indices merged into one pool");
	Check(pool.GetIndices()[3] == 3u, "second mesh indices rebased by baseVertex");
	Check(pool.GetMeshMetas()[1].first_index == 3u, "second mesh first_index offset");
	Check(pool.GetMeshMetas()[1].vertex_offset == 0, "vertexOffset stays 0 after rebase");
	Check(pool.GetMeshMetas()[0].bounds.w > 0.0f, "bounds radius computed");

	// --- scene: batch partitioning ---
	GPUSceneManager scene;
	const UInt32 mesh_of_object[5] = { 0u, 1u, 0u, 1u, 0u };   // deliberately interleaved
	for (UInt32 i = 0; i < 5; ++i)
	{
		GPUObjectData obj{};
		obj.mesh_id = mesh_of_object[i];
		obj.material_id = i % 2;
		scene.AddObject(obj);
	}
	Check(scene.GetObjectCount() == 5, "objects registered");

	scene.BuildBatches(pool);
	Check(scene.GetBatchCount() == 2, "one batch per used drawable unit");
	Check(scene.GetVisibleIDs().size() == 5, "visibleIDs sized to object count");

	const Vector<GPUBatch>& batches = scene.GetBatches();
	Check(batches[0].mesh_id == 0u && batches[0].base_instance == 0u && batches[0].instance_count == 3u,
		"batch0 covers mesh0 with 3 instances");
	Check(batches[1].mesh_id == 1u && batches[1].base_instance == 3u && batches[1].instance_count == 2u,
		"batch1 covers mesh1 with 2 instances at offset 3");

	const Vector<UInt32>& vis = scene.GetVisibleIDs();
	Check(vis[0] == 0u && vis[1] == 2u && vis[2] == 4u, "batch0 slice holds objects 0,2,4");
	Check(vis[3] == 1u && vis[4] == 3u, "batch1 slice holds objects 1,3");

	Bool no_holes = true;
	for (UInt32 v : vis) if (v == kInvalidIndex) no_holes = false;
	Check(no_holes, "visibleIDs has no holes");

	// --- instance stream: the culling shader's input ---
	Check(sizeof(GPUInstanceData) == 8, "GPUInstanceData is 8 bytes");
	const Vector<GPUInstanceData>& insts = scene.GetInstances();
	Check(insts.size() == 5, "one instance entry per object");
	Check(insts[0].object_id == 0u && insts[0].batch_id == 0u, "object0 maps to batch0");
	Check(insts[1].object_id == 1u && insts[1].batch_id == 1u, "object1 maps to batch1");
	Check(insts[4].object_id == 4u && insts[4].batch_id == 0u, "object4 maps to batch0");

	Bool batch_ids_valid = true;
	for (const GPUInstanceData& inst : insts)
		if (inst.batch_id >= scene.GetBatchCount()) batch_ids_valid = false;
	Check(batch_ids_valid, "every instance points at a real batch");

	// --- stage 3: instances inside a batch are ordered by material ---
	MeshPool pool2;
	pool2.AddMesh(MakeTrianglePayload());
	GPUSceneManager scene2;
	const UInt32 materials[3] = { 2u, 0u, 1u };   // deliberately out of order
	for (UInt32 i = 0; i < 3; ++i)
	{
		GPUObjectData obj{};
		obj.mesh_id = 0;
		obj.material_id = materials[i];
		scene2.AddObject(obj);
	}
	scene2.BuildBatches(pool2);
	const Vector<UInt32>& vis2 = scene2.GetVisibleIDs();
	Check(vis2.size() == 3, "sorted batch keeps every instance");
	Check(vis2[0] == 1u && vis2[1] == 2u && vis2[2] == 0u, "instances sorted by material id");

	// --- LOD groups ---
	MeshPool lod_pool;
	Vector<Tool::MeshDataPayload> lod_levels;
	lod_levels.push_back(MakeTrianglePayload());
	lod_levels.push_back(MakeTrianglePayload());
	const Vector<UInt32> lod_ids = lod_pool.AddMeshLODs(lod_levels);

	Check(lod_ids.size() == 1, "a LOD group reports one id (its LOD0)");
	Check(lod_pool.GetMeshCount() == 2, "both levels are registered");
	Check(lod_pool.GetLODGroupHeads().size() == 1, "they form ONE group, not two meshes");
	Check(lod_pool.GetLODGroupHeads()[0] == lod_ids[0], "the group head is LOD0");

	const Vector<GPUMeshMeta>& lod_metas = lod_pool.GetMeshMetas();
	Check(lod_metas[lod_ids[0]].lod_count == 2, "head records 2 levels");
	Check(lod_metas[lod_ids[0]].lod_offsets[0] == lod_ids[0]
		&& lod_metas[lod_ids[0]].lod_offsets[1] == lod_ids[0] + 1,
		"levels are contiguous after the head (batch == base + lod)");

	GPUSceneManager lod_scene;
	for (UInt32 i = 0; i < 4; ++i)
	{
		GPUObjectData obj{};
		obj.mesh_id = lod_ids[0];
		obj.material_id = i % 2;
		lod_scene.AddObject(obj);
	}
	lod_scene.BuildBatches(lod_pool);
	Check(lod_scene.GetBatchCount() == 2, "one batch per LOD level");
	Check(lod_scene.GetBatches()[0].mesh_id == lod_ids[0], "batch0 is LOD0 geometry");
	Check(lod_scene.GetBatches()[1].mesh_id == lod_ids[0] + 1, "batch1 is LOD1 geometry");
	Check(lod_scene.GetBatches()[0].max_instance_count == 4
		&& lod_scene.GetBatches()[1].max_instance_count == 4,
		"every level reserves the whole group (any instance may land anywhere)");
	Check(lod_scene.GetVisibleIDs().size() == 8, "visibleIDs reserves a slice per level");
	Check(lod_scene.GetInstances()[0].batch_id == 0, "instances point at the group base");
	Check(lod_scene.GetInstances()[3].batch_id == 0, "all instances share the group base");

	// A single-level group must still yield exactly one batch, so scenes with no
	// LOD assets behave as they did before LOD existed.
	MeshPool flat_pool;
	flat_pool.AddMesh(MakeTrianglePayload());
	GPUSceneManager flat_scene;
	for (UInt32 i = 0; i < 3; ++i)
	{
		GPUObjectData obj{};
		obj.mesh_id = 0;
		obj.material_id = i;
		flat_scene.AddObject(obj);
	}
	flat_scene.BuildBatches(flat_pool);
	Check(flat_scene.GetBatchCount() == 1, "single-level mesh yields one batch");
	Check(flat_scene.GetVisibleIDs().size() == 3, "single-level visibleIDs is not inflated");
	Check(flat_pool.GetMeshMetas()[0].lod_count == 1, "single-level head reports 1 level");

	// --- meshlet cluster build ---
	{
		// A unit cube: 8 vertices, 12 triangles. Split with a 4-vertex /
		// 4-triangle cap so the partition has to close several clusters.
		Vector<Tool::MeshVertex> cube_verts(8);
		for (UInt32 i = 0; i < 8; ++i)
		{
			cube_verts[i].position[0] = (i & 1u) ? 1.0f : 0.0f;
			cube_verts[i].position[1] = (i & 2u) ? 1.0f : 0.0f;
			cube_verts[i].position[2] = (i & 4u) ? 1.0f : 0.0f;
			cube_verts[i].normal[1] = 1.0f;
		}
		const UInt32 cube_idx[36] = {
			0,1,3, 0,3,2,  4,6,7, 4,7,5,  0,4,5, 0,5,1,
			2,3,7, 2,7,6,  0,2,6, 0,6,4,  1,5,7, 1,7,3
		};
		Vector<UInt32> idx(cube_idx, cube_idx + 36);

		MeshletBuilder::Result mr;
		MeshletBuilder::Build(cube_verts, idx, 0u, 0u, mr, 4u, 4u);

		Check(!mr.clusters.empty(), "meshlet build produced clusters");
		Check(mr.clusters.size() >= 3u, "12 triangles under a 4-triangle cap needs >= 3 clusters");
		Check(mr.cluster_draws.size() == mr.clusters.size(), "one draw record per cluster");
		Check(mr.cluster_draws[0].mesh_id == 0u, "draw records carry the mesh id");

		UInt32 total_tris = 0;
		Bool sizes_ok = true, refs_ok = true, tris_ok = true, bounds_ok = true;
		for (const GPUCluster& c : mr.clusters)
		{
			if (c.vertex_count == 0u || c.vertex_count > 4u) sizes_ok = false;
			if (c.triangle_count == 0u || c.triangle_count > 4u) sizes_ok = false;
			if (c.bounds.w <= 0.0f) bounds_ok = false;      // a cluster must span something
			total_tris += c.triangle_count;

			for (UInt32 i = 0; i < c.vertex_count; ++i)
				if (mr.vertex_refs[c.vertex_ref_offset + i] >= 8u) refs_ok = false;

			for (UInt32 i = 0; i < c.triangle_count * 3u; ++i)
				if (mr.triangles[c.triangle_offset + i] >= c.vertex_count) tris_ok = false;
		}
		Check(sizes_ok, "every cluster respects the vertex/triangle caps");
		Check(bounds_ok, "every cluster got real bounds");
		Check(refs_ok, "vertex refs point at real vertices");
		Check(tris_ok, "triangle indices stay inside their own cluster");
		Check(total_tris == 12u, "no triangle was lost while partitioning");

		// A cluster must be tighter than the mesh, otherwise culling it is no
		// better than culling the object.
		const GPUCluster& first = mr.clusters[0];
		Check(first.vertex_count < 8u, "clusters are proper subsets of the mesh");
	}

	// --- stage 4: soft delete + slot recycling ---
	MeshPool pool3;
	pool3.AddMesh(MakeTrianglePayload());
	GPUSceneManager scene3;
	Vector<GPUObjectHandle> handles3;
	for (UInt32 i = 0; i < 3; ++i)
	{
		GPUObjectData obj{};
		obj.mesh_id = 0;
		obj.material_id = i;
		handles3.push_back(scene3.AddObject(obj));
	}
	scene3.BuildBatches(pool3);
	Check(scene3.GetActiveObjectCount() == 3, "3 active before removal");
	Check(scene3.IsObjectAlive(handles3[1]), "handle is alive before removal");
	Check(scene3.RemoveObject(handles3[1]), "RemoveObject reports success");
	Check(!scene3.IsObjectAlive(handles3[1]), "removed handle is dead");
	Check(!scene3.RemoveObject(handles3[1]), "removing twice is rejected");
	Check(scene3.GetObjectByHandle(handles3[1]) == nullptr, "dead handle yields no object");

	scene3.BuildBatches(pool3);
	Check(scene3.GetActiveObjectCount() == 2, "2 active after removal");
	const Vector<UInt32>& vis3 = scene3.GetVisibleIDs();
	Check(vis3.size() == 2, "visibleIDs shrank to 2");
	Bool removed_gone = true;
	for (UInt32 v : vis3) if (v == handles3[1].index) removed_gone = false;
	Check(removed_gone, "removed object is absent from visibleIDs");
	Check(scene3.GetFreeSlotCount() == 1, "one slot is on the free list");

	// Recycling: the freed slot must be handed back out, not grown past.
	const UInt32 slots_before = scene3.GetSlotCount();
	GPUObjectData refill{};
	refill.mesh_id = 0;
	refill.material_id = 9;
	const GPUObjectHandle reused = scene3.AddObject(refill);
	Check(scene3.GetSlotCount() == slots_before, "recycled a freed slot instead of growing");
	Check(reused.index == handles3[1].index, "LIFO free list returns the most recently freed slot");
	Check(reused.generation != handles3[1].generation, "generation was bumped on recycle");
	Check(!scene3.IsObjectAlive(handles3[1]), "old handle stays dead once the slot is reused");
	Check(scene3.IsObjectAlive(reused), "the new handle is alive");
	Check(scene3.GetFreeSlotCount() == 0, "free list drained");

	scene3.BuildBatches(pool3);
	Check(scene3.GetActiveObjectCount() == 3, "3 active again after refill");

	// Churn: recycling must not consume slots no matter how many cycles run.
	for (UInt32 cycle = 0; cycle < 100; ++cycle)
	{
		const GPUObjectHandle h = scene3.AddObject(refill);
		scene3.RemoveObject(h);
	}
	Check(scene3.GetSlotCount() == slots_before + 1, "100 add/remove cycles consumed no new slots");

	// --- indirect args ---
	scene.BuildDrawCommands(pool);
	const Vector<DrawIndexedIndirectArgs>& cmds = scene.GetDrawCommands();
	Check(cmds.size() == 2, "one indirect command per batch");
	Check(cmds.size() == 2, "one indirect command per batch");
	Check(cmds[0].index_count == 3u && cmds[0].instance_count == 3u && cmds[0].first_index == 0u,
		"command0 matches mesh0 geometry");
	Check(cmds[1].index_count == 3u && cmds[1].instance_count == 2u && cmds[1].first_index == 3u,
		"command1 matches mesh1 geometry");
	Check(cmds[0].vertex_offset == 0 && cmds[1].vertex_offset == 0, "vertexOffset always 0 (rebased)");

	// --- stage 5: firstInstance carries the batch slice ---
	scene.SetUseFirstInstance(true);
	scene.BuildDrawCommands(pool);
	Check(scene.GetDrawCommands()[0].first_instance == 0u, "firstInstance 0 for batch0");
	Check(scene.GetDrawCommands()[1].first_instance == 3u, "firstInstance 3 for batch1");
	scene.SetUseFirstInstance(false);
	scene.BuildDrawCommands(pool);
	Check(scene.GetDrawCommands()[1].first_instance == 0u, "fallback zeroes firstInstance");

	std::cout << (g_failures == 0 ? "=== ALL PASS ===" : "=== FAILURES ===")
		<< " failures=" << g_failures << std::endl;
	return g_failures == 0 ? 0 : 1;
}

int main()
{
	if (std::getenv("GPUDRIVEN_SELFTEST") != nullptr)
		return RunSelfTest();

	GpuDrivenApp app;
	return MXRender::Application::SampleApp::RunSample(app, "Sample 16 - GPU Driven");
}
