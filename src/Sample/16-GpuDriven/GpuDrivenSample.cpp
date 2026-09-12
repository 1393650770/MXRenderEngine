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

using namespace MXRender;
using namespace MXRender::RHI;
using namespace MXRender::Render;
using namespace MXRender::Application;
using namespace MXRender::Render::GPUScene;

struct GpuDrivenPassData : public RenderGraphPassDataBase
{
	RenderPipelineState*  pso = nullptr;
	ShaderResourceBinding* srb = nullptr;
	~GpuDrivenPassData() override = default;
	void Release() override { delete srb; srb = nullptr; }
};

class GpuDrivenApp : public Application::SampleApp
{
public:
	GpuDrivenApp() = default;
	~GpuDrivenApp() override = default;

	void OnInitScene() override final;
	void OnShutdownScene() override final;
	void OnUpdate(float dt) override final;

protected:
	MeshPool  mesh_pool;
	GPUSceneManager  gpu_scene;
	Render::SceneView scene_view;
	Application::OrbitCameraController camera;
	UInt32 frame_index = 0;
	Float32 elapsed = 0.0f;
};

void GpuDrivenApp::OnInitScene()
{
	// ---- 1. merge geometry into one pool -------------------------------------
	// The same mesh is added twice on purpose: two drawable units means two
	// batches, which exercises visibleIDs slicing instead of trivially passing
	// with a single batch.
	Tool::MeshDataPayload payload;
	const Bool loaded = Tool::MeshLoader::LoadMeshData("Mesh/cube.obj", payload);
	ENSURE(loaded, "GpuDriven: failed to load Mesh/cube.obj");
	if (!loaded) return;

	mesh_pool.AddMesh(payload);
	mesh_pool.AddMesh(payload);
	ENSURE(mesh_pool.Upload(), "GpuDriven: failed to upload merged mesh pool");
	std::cout << "[GpuDriven] drawable units=" << mesh_pool.GetMeshCount()
		<< " verts=" << mesh_pool.GetVertexCount()
		<< " indices=" << mesh_pool.GetIndexCount() << std::endl;

	// ---- 2. scene data --------------------------------------------------------
	GPUSceneManager::Config cfg{};
	gpu_scene.Initialize(cfg, mesh_pool);

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
				// Alternate the drawable unit so both batches get instances.
				obj.mesh_id = static_cast<UInt32>((x + y + z) % 2);
				obj.material_id = static_cast<UInt32>((x + y + z) % 8);
				obj.sphere_bounds = mesh_pool.GetMeshMetas()[obj.mesh_id].bounds;
				gpu_scene.AddObject(obj);
			}

	gpu_scene.BuildBatches(mesh_pool);
	gpu_scene.BuildDrawCommands(mesh_pool);
	gpu_scene.UploadObjects();
	gpu_scene.UploadMaterials();
	gpu_scene.UploadVisibleIDs();
	gpu_scene.UploadDrawCommands();

	std::cout << "[GpuDriven] objects=" << gpu_scene.GetObjectCount()
		<< " materials=" << gpu_scene.GetMaterialCount()
		<< " batches=" << gpu_scene.GetBatchCount() << std::endl;

	// ---- 3. camera ------------------------------------------------------------
	camera.Attach(GetPlatformWindow());
	camera.distance = 22.0f;
	scene_view.SetPerspective(glm::radians(45.0f), 0.1f, 200.0f);

	// ---- 4. one pass, one binding, N indirect draws ---------------------------
	auto* pass = graph.AddRenderPass<GpuDrivenPassData>("GpuDrivenPass", &graph,
		RHIGetImmediateCommandList(),
		[&](GpuDrivenPassData& data, RenderGraphPassBuilder& builder, CommandList* in_cmd)
		{
			builder.Write(GetBackBufferResource());
			if (GetDepthStencilResource()) builder.Write(GetDepthStencilResource());

			Shader* vs = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Vertex,
				"Shader/gpuscene_object.vert.spv");
			Shader* ps = Tool::ShaderLibrary::LoadShader(ENUM_SHADER_STAGE::Shader_Pixel,
				"Shader/gpuscene_object.frag.spv");

			RenderGraphiPipelineStateDesc pd{};
			pd.shaders[ENUM_SHADER_STAGE::Shader_Vertex] = vs;
			pd.shaders[ENUM_SHADER_STAGE::Shader_Pixel] = ps;
			pd.primitive_topology = ENUM_PRIMITIVE_TYPE::TriangleList;
			pd.vertex_input_layout = Tool::MeshDataPayload::GetVertexInputLayout();
			pd.render_targets = { GetBackBuffer() };
			pd.depth_stencil_view = GetDepthStencil();
			pd.raster_state.sample_count = 1;
			pd.raster_state.cull_mode = ENUM_RASTER_CULLMODE::Back;
			pd.depth_stencil_state.depth_test_enable = true;
			pd.depth_stencil_state.depth_write_enable = true;
			pd.depth_stencil_state.depth_func = ENUM_STENCIL_FUNCTION::ENUM_LESS;
			pd.blend_state.render_targets.resize(1);
			data.pso = g_render_rhi->CreateRenderPipelineState(pd);

			// false (not static): SetResource still applies. This happens ONCE —
			// never per frame, because two frames can be in flight and rewriting
			// a live descriptor set corrupts the previous frame.
			data.pso->CreateShaderResourceBinding(data.srb, false);
			data.srb->SetResource("g_scene", gpu_scene.GetUniformBuffer());
			data.srb->SetResource("g_objects", gpu_scene.GetObjectBuffer());
			data.srb->SetResource("g_materials", gpu_scene.GetMaterialBuffer());
			data.srb->SetResource("g_meshes", gpu_scene.GetMeshBuffer());
			data.srb->SetResource("g_visible", gpu_scene.GetVisibleIDBuffer());
			data.srb->FlushDescriptorWrites();

			// PSO cache hashes shader pointers: delete shaders only after all PSOs
			// are created (CLAUDE.md RHI Gotchas).
			delete vs;
			delete ps;
		},
		[this](CONST GpuDrivenPassData& data, CommandList* in_cmd)
		{
			BindBackBufferTarget(in_cmd);
			in_cmd->SetGraphicsPipeline(data.pso);
			in_cmd->SetShaderResourceBinding(data.srb);
			in_cmd->SetVertexBuffer(mesh_pool.GetVertexBuffer(), 0, MeshPool::GetVertexStride(), 0);
			in_cmd->SetIndexBuffer(mesh_pool.GetIndexBuffer(), 0, true);

			// Everything below is per-batch, never per-object — and none of it is
			// a descriptor binding.
			const UInt32 stride = static_cast<UInt32>(sizeof(DrawIndexedIndirectArgs));
			const Vector<GPUBatch>& batches = gpu_scene.GetBatches();
			for (UInt32 b = 0; b < batches.size(); ++b)
			{
				// Batch slice offset. Needed only because shaderDrawParameters is
				// off, so firstInstance in the args is ignored.
				const UInt32 base = batches[b].base_instance;
				in_cmd->SetPushConstants(0, sizeof(UInt32), &base);
				in_cmd->DrawIndexedIndirect(gpu_scene.GetDrawCommandBuffer(), b * stride, 1);
			}
		});
	pass->SetIsCullable(false);
	pass->SetShaderPath("Shader/gpuscene_object");
}

void GpuDrivenApp::OnShutdownScene()
{
	// Release before RHIShutdown — member destruction runs too late.
	gpu_scene.Shutdown();
	mesh_pool.Release();
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
	u.counts.x = gpu_scene.GetObjectCount();
	u.counts.y = frame_index++;
	gpu_scene.UploadUniforms();

	// Slow spin. Only the 96-byte object record is rewritten — no descriptor
	// touch, which is the entire point.
	for (UInt32 i = 0; i < gpu_scene.GetObjectCount(); ++i)
	{
		GPUObjectData& obj = gpu_scene.GetObject(i);
		const glm::vec3 pos = glm::vec3(obj.model[3]);
		obj.model = glm::translate(glm::mat4(1.0f), pos)
			* glm::rotate(glm::mat4(1.0f), elapsed * 0.35f + i * 0.01f, glm::vec3(0.0f, 1.0f, 0.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(0.45f));
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

	// --- indirect args ---
	scene.BuildDrawCommands(pool);
	const Vector<DrawIndexedIndirectArgs>& cmds = scene.GetDrawCommands();
	Check(cmds.size() == 2, "one indirect command per batch");
	Check(cmds[0].index_count == 3u && cmds[0].instance_count == 3u && cmds[0].first_index == 0u,
		"command0 matches mesh0 geometry");
	Check(cmds[1].index_count == 3u && cmds[1].instance_count == 2u && cmds[1].first_index == 3u,
		"command1 matches mesh1 geometry");
	Check(cmds[0].vertex_offset == 0 && cmds[1].vertex_offset == 0, "vertexOffset always 0 (rebased)");

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
