<p align="center">
<!-- TODO: add logo -->
<br>
</p>

<h1 align="center">MXRender Engine</h1>

<p align="center">
  <a href="https://github.com/1393650770/MyRenderer/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT">
  </a>
  <a href="https://isocpp.org/">
    <img src="https://img.shields.io/badge/language-C%2B%2B20-%23f34b7d.svg" alt="Language: C++20">
  </a>
  <a href="https://www.vulkan.org/">
    <img src="https://img.shields.io/badge/API-Vulkan%201.3-%23a41e22.svg" alt="API: Vulkan">
  </a>
  <a href="#">
    <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Android%20%7C%20WebAssembly-lightgrey.svg" alt="Platform: Windows | Android | WebAssembly">
  </a>
  <a href="#">
    <img src="https://img.shields.io/badge/status-active%20development-brightgreen.svg" alt="Status: Active Development">
  </a>
</p>

<p align="center">
  <strong>A real-time rendering engine powered by C++20 and Vulkan</strong> — featuring a declarative <strong>RenderGraph</strong> with visual node editor, <strong>GPU neural network</strong> training/inference via compute shaders, <strong>bindless rendering</strong>, and a <strong>multi-backend RHI</strong> targeting Windows, Android, and WebAssembly.
</p>

---

**Language**: [English](#english) | [中文](#中文)

---

<a name="english"></a>

# 🇬🇧 English

## 📑 Table of Contents

- 🎯 What is MXRender?
- 🏗️ Architecture
- ✨ Features (RHI · RenderGraph · Node Editor · GPU Neural Network · Bindless · Rendering Techniques · Reflection · UI Framework · Multi-Threading · Cross-Platform)
- 🚀 Getting Started
- 📖 API at a Glance
- 🧪 Samples
- 🗺️ Project Structure
- 🤝 Contributing
- 📄 License
- 🙏 Acknowledgments

## 🎯 What is MXRender?

MXRender is a **real-time rendering research platform** — not a game engine, but a playground for exploring modern real-time rendering techniques from the ground up. Every subsystem (RHI, RenderGraph, shader pipeline, even the neural network framework) is built from scratch with full visibility into the GPU command stream.

It is developed as a solo engine project, prioritizing **architectural clarity** and **correctness** over feature quantity. The codebase is structured with strict one-way layering — each layer knows nothing about the one above it — and a philosophy that correctness rules should be **embedded in the system**, not passed around as comments.

> **Key Numbers**: 30+ Vulkan backend files · 31 GPU compute shaders for neural network training · 14 runnable samples · 3 RHI backends · 3 thread modes · 256-level undo/redo · 4096-texture bindless heap

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Editor.exe          Visual RenderGraph Editor (ImGui)  │
│  src/Editor/         Node canvas · Undo/Redo · Templates │
├─────────────────────────────────────────────────────────┤
│  Runtime.lib         Engine Core Library                 │
│  src/Runtime/                                            │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐ │
│  │   RHI    │  │  Render  │  │   UI     │  │  Asset  │ │
│  │ Vulkan   │  │  Graph   │  │ RmlUI    │  │  Mesh   │ │
│  │ WGPU     │  │ Compile  │  │ Widget   │  │ Texture │ │
│  │ GLES3    │  │ Execute  │  │ Binding  │  │ glTF    │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘ │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌─────────┐ │
│  │ Platform │  │ Network  │  │  Audio   │  │  Input  │ │
│  │ Desktop  │  │   KCP    │  │ Browser  │  │ Action  │ │
│  │ Android  │  │ Browser  │  │ WeChat   │  │  Map    │ │
│  │Emscripten│  │ Desktop  │  │ Douyin   │  │         │ │
│  │ WeChat   │  │          │  │          │  │         │ │
│  │ Douyin   │  │          │  │          │  │         │ │
│  └──────────┘  └──────────┘  └──────────┘  └─────────┘ │
├─────────────────────────────────────────────────────────┤
│  ThirdParty        Vulkan · GLFW · ImGui · Boost · ...  │
└─────────────────────────────────────────────────────────┘
```

**Strict one-way layering rules** (enforced at compile time):
- **Runtime never includes Editor headers** — `src/Editor/` is excluded from Runtime's include path
- **Render layer never includes Vulkan headers** — all GPU operations go through the RHI abstraction
- **Editor headers never include Vulkan headers** — ImGui backend `.cpp` is the only exception
- **UI abstraction never includes backend headers** — `UIManager.h`, `UISystem.h`, `UIDataModelBinder.h` are RmlUi/GLFW-free

## ✨ Features

### 1️⃣ Render Hardware Interface (RHI)

A platform-agnostic GPU abstraction inspired by UE's RHI design. Three backends coexist under the same interface:

| Backend | Status | Files | Platform |
|---------|--------|-------|----------|
| **Vulkan** | ✅ Primary | 30+ (VK_*) | Windows, Android |
| **WebGPU** | 🧪 Experimental | 14 (WGPU_*) | Browser / WASM |
| **GLES3** | ✅ MiniGame-ready | 19 (GLES3_*) | WASM / WebGL2 / WeChat / Douyin |

All backends share the same abstract commands: `CreateTexture`, `CreateBuffer`, `SetGraphicsPipeline`, `Draw`, `Dispatch`, `ResourceBarrier`, `TransitionTextureState`, etc. Capability differences are queried at runtime via `DeviceProfile` (bindless support, dynamic rendering, compressed texture formats, unified memory, VRAM size).

**Key capabilities exposed through the abstraction**:
- **Dynamic Rendering** (Vulkan 1.3 `vkCmdBeginRendering`) with automatic fallback to legacy render passes
- **Tessellation**: `PatchList` topology + hull/domain shader stages
- **Indirect Draw / Dispatch**: `DrawIndirect`, `DrawIndexedIndirect`, `DispatchIndirect` with `Storage|Indirect` buffer support
- **Storage Images (UAV)**: `image2D`/`uimage2D` compute writes with `imageAtomicMin/Add`
- **GPU Memory Sub-Allocation**: UE-style three-tier system — `DeviceMemoryManager` (pages) → `MemoryResourceFragmentAllocator` (sub-allocation) → `ResourcePool` (transient reuse by descriptor hash)
- **Pipeline Cache Persistence**: `PipelineCache.bin` loaded at startup, saved at shutdown; PSOs are content-hash cached
- **Shader Reflection Binding**: SPIRV-Reflect drives descriptor set layouts and by-name resource binding (`srb->SetResource("name", ...)`)

### 2️⃣ RenderGraph System

A declarative frame graph inspired by [Frostbite's framegraph](https://www.ea.com/frostbite/news/framegraph-extensible-rendering-architecture-in-frostbite). Passes declare their inputs and outputs; the system automatically handles barriers, transient memory aliasing, and execution order.

**Compile pipeline**:

```
Reference   →  Culling   →  Topological  →  Timeline   →  Split Barrier  →  Transient
Counting        (ref=0)      Sort (+cycle    Scheduling     Generation         Memory Aliasing
                             detection)
```

- **Declarative API**: `graph.AddRenderPass<MyData>("Name", setup_lambda, execute_lambda)` — the setup lambda declares resource dependencies via `builder.Read(res)` / `builder.Write(res)`, the execute lambda records GPU commands
- **Async Compute Scheduling**: Passes tagged for compute queue are automatically dispatched on the async queue with correct cross-queue barriers
- **Blackboard**: Type-safe cross-pass CPU data sharing (`graph.GetBlackboard().Set<Matrix4>("ViewProj", mat)`)
- **Serialization**: Full `.rgraph.json` export/import (schema v2) — every sample exports its graph definition, loadable into the visual editor
- **GraphViz Export**: `graph.Compile()` can dump the DAG as a `.dot` file for visualization

### 3️⃣ Visual Node Editor

An ImGui-based (docking + multi-viewport) visual programming environment for building RenderGraphs. Built on `ax::NodeEditor`.

| Component | Description |
|-----------|-------------|
| **Node Canvas** | Drag-and-drop pass/resource nodes, draw links between pins, right-click context menus driven by a built-in `PassRegistry` |
| **Undo/Redo** | Full Command pattern with 256-level history, transaction grouping (`BEGIN_TXN`/`END_TXN`), and command merging for drag operations |
| **Pass Templates** | Built-in library: GBuffer, DepthPre, Shadow, Lighting, SSAO, Bloom, TAA, ToneMapping — plus user-defined `.rgtemplate.json` files |
| **Validation** | Two-tier: editor-side `GraphValidator` (naming, connectivity) + runtime `RenderGraphValidator` (cycles, pin types, bipartite rule) |
| **Auto-Save** | Periodic atomic save (write-to-tmp-then-rename) with crash recovery detection on next launch |
| **Sub-Graph Nodes** | Collapsible sub-graph grouping with exposed input/output pins |
| **Properties Panel** | Selection-synced property editor with format combo boxes |
| **Outline Panel** | Tree-view of all passes and resources, synchronized across panels via `EditorEventBus` |
| **Search** | Case-insensitive substring search across the entire node graph |

### 4️⃣ GPU Neural Network (NNE)

A full neural network training and inference framework that runs **entirely on the GPU** via Vulkan compute shaders — no CPU math, no CUDA, no third-party ML libraries.

**31 compute shaders** implement the complete training loop:

```
Forward Pass → Loss Computation → Backward Pass → Gradient Update
      ↑                                                  │
      └──────────── next epoch / next batch ─────────────┘
```

| Category | Implementations |
|----------|----------------|
| **Layers** | Linear (Fully Connected), Conv2D, MultiHeadAttention, BatchNorm1D, LayerNorm, Dropout, ResidualBlock |
| **Activations** | ReLU, LeakyReLU, Sigmoid, Tanh, GELU, SiLU — each with dedicated forward/backward compute shaders |
| **Loss** | SoftmaxCrossEntropy |
| **Optimizers** | SGD (with momentum), Adam, AdamW |
| **Schedulers** | StepLR, CosineAnnealingLR |
| **Initializers** | GlorotUniform, HeNormal, HeUniform |

**Three runnable demonstrations**:
1. **MLP Spiral Classification** — 2→64→3 architecture classifying a 2D spiral dataset, all training in compute shaders
2. **MNIST CNN** — Conv2D→BatchNorm→ReLU→Conv2D→BatchNorm→ReLU→Dropout→Linear→Linear→SoftmaxCrossEntropy with AdamW and model save/load
3. **Tiny Transformer** — MultiHeadAttention→LayerNorm→Residual→GELU with CosineAnnealingLR, demonstrating the attention mechanism on synthetic data

### 5️⃣ Bindless Rendering

A global descriptor set (`set = 2`) with full descriptor indexing support:

```
┌──────────────────────────────────────────────────┐
│  Global Descriptor Heap (Set 2)                   │
│  ┌────────────────────┐  ┌──────────┐  ┌───────┐ │
│  │  4096 Texture2D    │  │ 256 Cube │  │ 64    │ │
│  │  slots             │  │ slots    │  │ Smplr │ │
│  └────────────────────┘  └──────────┘  └───────┘ │
└──────────────────────────────────────────────────┘
```

- **Slot Management**: `AllocateTexture2DSlot()` / `FreeTexture2DSlot()` with generation-protected handles — stale handles are detected and return `nullptr`, not garbage
- **Shader Integration**: Textures are sampled by index — `bindless_textures[nonuniformEXT(idx)]` in GLSL
- **Material Integration**: `BindlessMaterialData` maps material texture references to bindless heap indices

### 6️⃣ Rendering Techniques

Each technique is implemented as a self-contained sample with its own shader suite:

| Technique | Sample | Key Shaders | Highlights |
|-----------|--------|-------------|------------|
| **2D Stable Fluids** | `Fluid2D` | `fluid_advect`, `fluid_divergence`, `fluid_jacobi`, `fluid_project`, `fluid_splat` | Mouse-interactive ink-wash stylized rendering; Jacobi pressure solve (24 iterations); all on storage buffers |
| **3D FLIP Water** | `Fluid3D` | `fluid3d_advect`, `fluid3d_p2g`, `fluid3d_pressure`, `fluid3d_update`, `fluid3d_display` | MAC-grid pressure projection (64 Jacobi iterations); fixed-point-atomic P2G; GPU free-list particle recycling; screen-space water rendering with foam; 6 RDG passes with retained storage-image textures |
| **FFT Ocean** | `Ocean` | `ocean_init_spectrum`, `ocean_fft_rows`, `ocean_fft_cols`, `ocean_unpack`, `ocean_buffer_export` | Phillips spectrum → IFFT chain; storage-buffer export to graphics PSO; vertex-pulling grid (512×512); wind/choppiness/foam parameters |
| **Volumetric Cloud** | `VolumetricCloud` | `cloud_noise_shape`, `cloud_atmo_skyview`, `cloud_march`, `cloud_filter`, `cloud_taa`, `cloud_composite` | Hillaire sky LUT + Nubis-style raymarch; one-shot compute bake of tileable 3D Perlin-Worley noise; per-frame sky-view LUT + half-res cloud march; `sampler3D` in compute and fragment stages |
| **PBR Mesh** | `Mesh` | Standard PBR vertex/fragment | `.obj` load → `Vertex\|Dynamic` VB/IB; OrbitCameraController; DrawIndexed ↔ DrawIndexedIndirect toggle |

### 7️⃣ Reflection & Code Generation

A libclang-based meta-parser scans annotated C++ headers and generates code via Mustache templates — all as a pre-build step (`MetaParser.exe`):

```
Annotated .h  →  MetaParser (libclang)  →  Mustache Templates  →  Generated .cpp/.h
                                                                    ↓
                                          ┌─────────────────────────┼─────────────────────────┐
                                          │                         │                         │
                                     RTTR Registration        JSON Serializer          UI Binding Code
                                  (property get/set,        (load/save entire       (UIWidgetBindingTraits<T>
                                   method invoke)            object trees)            for UI_BIND fields)
```

- **RTTR Integration**: Classes annotated with `MYRENDERER_BEGIN_CLASS`/`MYRENDERER_END_CLASS` get automatic `rttr::registration` — properties and methods are introspectable at runtime
- **Serialization**: Full object-tree JSON save/load — the visual editor's save files are generated by the serializer code-gen
- **UI Binding**: `UI_BIND(Enable, TWO_WAY, FIELD_AS=hp)` field annotations auto-generate `UIWidgetBindingTraits<T>` specializations — zero boilerplate for connecting C++ data to RmlUI documents

### 8️⃣ Multi-Backend UI Framework

A cleanly layered UI system where the application layer sees only a single facade (`UIManager`) and never touches backend types:

```
Application (Sample/Editor)
    │  #include "UI/UIManager.h"       ← Singleton facade (zero backend includes)
    ▼
UIManager                              ← Holds UISystem*, dispatches polymorphically
    │
    ▼
UISystem (abstract backend)            ← Virtual methods with default empty implementations
    │
    └── RmlUISystem (concrete)         ← Owns RmlUi context, renderer, input, data models
         ├── UIRenderer (3 virtuals)   ← BeginFrame / EndFrame / NeedsOffscreen
         ├── UIDataModelBinder         ← Typed binding: Bind("hp", &Int) — no void*, no Rml types
         └── UIInputBridge             ← Mouse / keyboard / scroll / text input abstraction
```

- **RmlUI Backend**: Full HTML/CSS UI middleware integration — load `.rml` documents, bind C++ data models, handle events
- **Widget Framework**: RTTR-driven declarative widgets with `UI_BIND(Enable, ...)` field annotations — create a `UIWidgetManager`, call `CreateWidget<GameHUD>()`, and `SynchronizeAll()` each frame
- **Sample Demo**: `12-RmlUI` demonstrates HP bars, score counters, timers, and event handlers — the Sample layer includes **zero** `<RmlUi/...>` headers

### 9️⃣ Multi-Threaded Rendering

Three switchable threading modes backed by a triple-buffered `FrameSynchronizer`:

| Mode | Threads | Description |
|------|---------|-------------|
| `Single` | 1 | Logic + Render + RHI all on the main thread |
| `RHIThread` | 2 | Logic+Render on main thread, RHI submits on a dedicated thread |
| `ThreeThread` | 3 | Logic, Render, and RHI each on their own thread — maximum parallelism |

Plus a vendored `TaskScheduler` with fiber-based work stealing, grouped tasks, and profiler integration — used for asset loading and parallel-for workloads.

### 🌍 Cross-Platform

| Platform | Graphics Backend | Status |
|----------|-----------------|--------|
| **Windows** | Vulkan | ✅ Primary development target |
| **Android** (ARM64) | Vulkan | ✅ Cross-compilation + APK packaging verified; rendering under investigation |
| **WebAssembly** | WebGL 2.0 (GLES3) | ✅ MiniGame samples verified |
| **WeChat MiniGame** | WebGL 2.0 (GLES3) | ✅ Pack scripts ready |
| **Douyin MiniGame** | WebGL 2.0 (GLES3) | ✅ Pack scripts ready |

Android build chain: NDK r27+ -> xmake cross-compile -> .so -> APK.

```batch
# One-click build
build_android.bat

# Or manual
xmake f -p android -a arm64-v8a --ndk=D:/path/to/ndk --ndk_sdkver=26 -y
xmake build RendererSample-HelloTriangle

# Package APK (configure SDK/JDK paths in pack/Android/build_apk.bat first)
.\pack\Android\build_apk.bat
```

All toolchain paths in `.xmake/paths.ini`.

### 🎮 MiniGame Build (WeChat / Douyin / Browser)

**Prerequisites**: Emscripten SDK 6.0.4+ installed at `src/ThirdParty/emsdk/`.

```batch
# Build all 3 WASM samples (HelloTriangle, Texture, Mesh)
build_wasm.bat

# Browser preview
cd build\wasm\wasm32\debug
python -m http.server 8080
# → http://localhost:8080/MiniGame-Mesh.html
```

**Package for WeChat**:
```batch
pack\WeChat\build_pack.bat MiniGame-Mesh debug
# Output: pack\WeChat\dist\MiniGame-Mesh_wx\
# → Import into WeChat Developer Tools → Compile → Preview
```

**Package for Douyin**:
```batch
pack\Douyin\build_pack.bat MiniGame-Mesh debug
# Output: pack\Douyin\dist\MiniGame-Mesh_tt\
# → Import into Douyin Developer Tools → Compile → Preview
```

**MiniGame Samples**:
| Target | Feature |
|--------|---------|
| `MiniGame-HelloTriangle` | Hardcoded triangle via gl_VertexID |
| `MiniGame-Texture` | Fullscreen quad + checkerboard texture sampling |
| `MiniGame-Mesh` | Rotating cube VBO+VAO+UBO + mouse/touch orbit camera |

## 🚀 Getting Started

### Prerequisites

- **[xmake](https://github.com/xmake-io/xmake)** v2.9.2+ — cross-platform build system
- **Visual Studio 2022/2026** — with C++20 support
- **Vulkan SDK** — automatically fetched by xmake

### One-Click Build

Double-click `gen_vs_sln.bat` or run in terminal:

```batch
gen_vs_sln.bat
```

This script will:
1. Patch gli + imgui xmake recipes (fixes known compatibility issues)
2. Clean stale caches
3. Generate a Visual Studio `.sln` with debug / release / releasedbg configurations (x64)

> **Users in China**: If GitHub downloads fail, set a proxy first:
> ```batch
> set HTTP_PROXY=http://127.0.0.1:10809
> gen_vs_sln.bat
> ```

### Manual Build

```batch
# Install xmake first: https://github.com/xmake-io/xmake/releases

# Generate VS solution
xmake project -k vsxmake -m "debug;release;releasedbg" -a x64 -y

# Build from command line
xmake f -m debug -y
xmake build Editor
```

### Run a Sample

```batch
xmake build RendererSample-HelloTriangle
xmake run RendererSample-HelloTriangle
```

> **Important**: Always use `xmake run` — it sets the working directory and injects DLL paths. Launching the `.exe` directly will fail.

Shaders are compiled to SPIR-V automatically as a pre-build step (`glslangValidator`), and copied into the output directory alongside textures and DLLs.

### Android Build (Cross-Compile)

```batch
# 1. Set NDK path in .xmake/android_ndk.txt (one line):
#    D:/path/to/android-ndk-r27d

# 2. Configure & build
xmake f -p android -a arm64-v8a --ndk_sdkver=26 -y
xmake build RendererSample-HelloTriangle

# 3. Package APK (configure SDK/JDK paths in pack/Android/build_apk.bat first)
.\pack\Android\build_apk.bat

# 4. Install
adb install -r .\pack\Android\mxrender_hello.apk
```

## 📖 API at a Glance

### Minimal RenderGraph Pass (Hello Triangle)

```cpp
#include "Application/SampleApp.h"
#include "Render/Core/RenderGraph.h"

struct TriPassData : public RenderGraphPassDataBase {
    PSOHandle pipeline_state;
    ShaderResourceBinding* srb = nullptr;
};

class MyApp : public SampleApp {
    void OnInitScene() override {
        auto* pass = graph.AddRenderPass<TriPassData>("MainPass",
            // Setup: declare dependencies + create PSO
            [&](TriPassData& data, RenderGraphPassBuilder& builder, CommandList* cmd) {
                builder.Write(GetBackBufferResource());    // Declare output

                auto* vs = ShaderLibrary::LoadShader(Shader_Vertex, "Shader/my.vert.spv");
                auto* ps = ShaderLibrary::LoadShader(Shader_Pixel,  "Shader/my.frag.spv");

                RenderGraphiPipelineStateDesc pso_desc;
                pso_desc.shaders[Shader_Vertex] = vs;
                pso_desc.shaders[Shader_Pixel]  = ps;
                pso_desc.render_targets = { GetBackBuffer() };
                // ... fill raster_state, blend_state ...

                data.pipeline_state = Create<PSOHandle>(pso_desc, "MyPSO");
                Resolve(data.pipeline_state)->CreateShaderResourceBinding(data.srb);
            },
            // Execute: record draw commands
            [=](const TriPassData& data, CommandList* cmd) {
                BindBackBufferTarget(cmd);
                cmd->SetGraphicsPipeline(Resolve(data.pipeline_state));
                cmd->SetShaderResourceBinding(data.srb);
                cmd->Draw({ .vertexCount = 3, .instanceCount = 1 });
            });

        pass->SetIsCullable(false);   // Keep pass even with no external outputs
    }

    void OnShutdownScene() override {
        SaveGraphDefinition("MyGraph", "my_graph.rgraph.json"); // Export for Editor
    }
};

int main() {
    MyApp app;
    return SampleApp::RunSample(app, "My Sample");
}
```

### Compute Pass with Barriers

```cpp
// Inside a setup lambda:
builder.Create(storage_tex, TextureDesc{...}, "ResultTex");
builder.Write(storage_tex, ENUM_TEXTURE_STATE::UnorderedAccess);

// Inside the execute lambda:
cmd->SetComputePipeline(Resolve(compute_pso));
cmd->SetShaderResourceBinding(compute_srb);
cmd->Dispatch(thread_groups_x, thread_groups_y, 1);

// Barrier: UAV → SRV for the next pass
cmd->ResourceBarrier(ENUM_TEXTURE_STATE::UnorderedAccess,
                     ENUM_TEXTURE_STATE::ShaderResource);
```

### Adding a UI

```cpp
#include "UI/UIManager.h"
#include "UI/RmlUI/RmlUISystem.h"

// Init
auto* rml = new RmlUI::RmlUISystem();
rml->Init(viewport);
UIManager::Create(rml);
UIManager::Get().LoadFont("RmlUI/font.ttf");

// Bind data model
auto model = UIManager::Get().CreateDataModel("hud");
UIManager::Get().BindDataModel<UIWidgetBindingTraits<GameHUD>>(model, &hud_data);

// Load and show panel
auto doc = UIManager::Get().LoadPanel("RmlUI/hud.rml");
UIManager::Get().ShowPanel(doc);

// Per frame
UIManager::Get().Update(delta_time);
UIManager::Get().DirtyVariable(model, "hp");   // Notify changed fields
UIManager::Get().Render(command_list);          // Record UI draw commands

// Shutdown
UIManager::Destroy();   // Calls m_backend->Shutdown() + delete
```

## 🧪 Samples

Each sample is a self-contained `.cpp` file demonstrating a specific subsystem or technique. Run any sample with `xmake build <target> && xmake run <target>`.

| # | Target | Concept | Shader Suite |
|---|--------|---------|-------------|
| 1 | `RendererSample-HelloTriangle` | Minimal RenderGraph pass | `triangle_test.vert/.frag` |
| 2 | `RendererSample-Texture` | Async DDS texture loading + sampling | `texture_test.*` |
| 3 | `RendererSample-CubeMap` | DDS cubemap skybox | `cubemap.*` |
| 4 | `RendererSample-Reflection` | MetaParser + RTTR property/method reflection | *(console app, no shaders)* |
| 5 | `RendererSample-Bindless` | Bindless PBR with material system | `bindless.*` |
| 6 | `RendererSample-NeuralNetwork` | GPU MLP — spiral classification | `nn_*.comp` (31 shaders) |
| 7 | `RendererSample-NNE_MNIST_CNN` | GPU CNN — synthetic MNIST | `nn_*.comp` |
| 8 | `RendererSample-NNE_Transformer` | GPU Transformer — self-attention | `nn_*.comp` |
| 9 | `RendererSample-Fluid2D` | 2D stable fluids with ink rendering | `fluid_*.comp/.frag` |
| 10 | `RendererSample-Fluid3D` | 3D FLIP water with foam (6 RDG passes) | `fluid3d_*.comp/.frag` |
| 11 | `RendererSample-Ocean` | FFT spectral ocean waves | `ocean_*.comp/.vert/.frag` |
| 12 | `RendererSample-VolumetricCloud` | Hillaire sky + Nubis raymarched clouds | `cloud_*.comp/.frag/.vert` |
| 13 | `RendererSample-Mesh` | .obj loading + PBR + indirect draw | Standard PBR + `gpu_culling.comp` |
| 14 | `RendererSample-RmlUI` | Game HUD with data binding + events | *(UI rendering via RmlUI)* |
| 15 | `RendererSample-VirtualTexture` | Quad-tree page layout prototype | *(WIP)* |

<!-- TODO: add screenshots of key samples -->

## 🗺️ Project Structure

```
MyRenderer/
├── src/
│   ├── Runtime/                 # Engine library
│   │   ├── Core/                # Base types (ConstDefine.h), ResourceHandle, reflection
│   │   ├── RHI/                 # RHI abstraction + ResourceManager
│   │   │   ├── Vulkan/          #   VK_* backend (30+ files)
│   │   │   ├── WGPU/            #   WGPU_* backend (experimental)
│   │   │   └── GLES3/           #   GLES3_* backend (mini-game / WebGL2)
│   │   ├── Render/Core/         # RenderGraph, VirtualTexture, BindlessMaterial
│   │   ├── Application/         # Window (GLFW), SampleApp, CameraController
│   │   ├── Asset/               # MeshAsset, TextureAsset, glTF loader, AssetPack
│   │   ├── Platform/            # Platform: Desktop/Android/Emscripten/WeChat/Douyin
│   │   ├── UI/                  # UIManager, UISystem, UIRenderer (abstract)
│   │   │   ├── RmlUI/           #   RmlUI concrete backend
│   │   │   └── Widget/          #   RTTR-based declarative widget framework
│   │   └── Tool/                # ShaderLibrary, BufferUtils, ComputeUtils
│   ├── Editor/                  # Editor application
│   │   ├── EditorRender/        # EditorRenderPipeline + EditorUI (ImGui)
│   │   └── UI/RenderGraphEditor/# Visual node editor + services
│   ├── Sample/                  # 15 self-contained samples
│   ├── Reflect/meta_parser/     # libclang code-gen tool
│   └── _Generated/              # Auto-generated code (do not edit)
├── resource/                    # Shaders (GLSL + SPIR-V), textures, editor assets
├── template/                    # Mustache templates for code-gen
├── pack/Android/                # APK packaging scripts
├── gen_vs_sln.bat               # One-click VS solution generator
└── xmake.lua                    # Build configuration
```

## 🤝 Contributing

This is a solo engine project, but contributions and feedback are welcome!

### Branch Strategy

| Branch | Purpose |
|--------|---------|
| `main` | Release-ready code |
| `main_dev` | Integration branch for feature branches |
| `rendergraph-editor` | Most active development (RenderGraph + Editor + Samples) |
| Feature branches | One feature per branch, merged into `main_dev` |

### Development Conventions

The codebase follows strict conventions documented in [CLAUDE.md](CLAUDE.md):
- **Namespaces**: `MXRender::RHI::`, `MXRender::Render::`, `MXRender::UI::`, etc.
- **Macros**: `MYRENDERER_BEGIN_CLASS`, `VIRTUAL`, `OVERRIDE`, `CHECK_WITH_LOG` (fires on `TRUE` — inverted vs standard assert)
- **Types**: UE-style aliases (`UInt32`, `String`, `Vector<T>`, `UniquePtr<T>`)
- **Encoding**: GBK for `.cpp`/`.h` files

Before submitting a PR, please verify the layering rules:

```bash
grep -r '#include.*vulkan' src/Runtime/Render/ | wc -l   # Must be 0
grep -r '#include.*Editor' src/Runtime/ | wc -l          # Must be 0
grep -r '#include.*VK_' src/Runtime/Render/ | wc -l      # Must be 0
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for more details. <!-- TODO: create CONTRIBUTING.md -->

## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

```
MIT License
Copyright (c) 2022 1393650770
```

## 🙏 Acknowledgments

MXRender builds on the shoulders of these excellent projects and libraries:

### Third-Party Libraries

**Via xmake packages**: [Vulkan SDK](https://www.vulkan.org/) · [GLFW](https://www.glfw.org/) · [GLM](https://github.com/g-truc/glm) · [Dear ImGui](https://github.com/ocornut/imgui) (docking branch) · [Assimp](https://github.com/assimp/assimp) · [Boost](https://www.boost.org/) · [FlatBuffers](https://google.github.io/flatbuffers/) · [glslang](https://github.com/KhronosGroup/glslang) · [RTTR](https://github.com/rttrorg/rttr) · [nlohmann/json](https://github.com/nlohmann/json) · [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) · [GLI](https://github.com/g-truc/gli) · [LZ4](https://github.com/lz4/lz4) · [Optick](https://github.com/bombomby/optick) · [Freetype](https://www.freetype.org/)

**Vendored in-tree**: [RmlUi](https://github.com/mikke89/RmlUi) · [TaskScheduler](https://github.com/dougbinks/enkiTS) · [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect) · [stb_image](https://github.com/nothings/stb) · [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) · [imgui-node-editor](https://github.com/thedmd/imgui-node-editor) · [KCP](https://github.com/skywind3000/kcp) · [tinygltf](https://github.com/syoyo/tinygltf) · [Emscripten SDK](https://github.com/emscripten-core/emsdk)

### Design Inspiration

- **Unreal Engine** — RHI abstraction design, memory sub-allocation model, descriptor binding patterns
- **[Nvrhi](https://github.com/GPUOpen-LibrariesAndSDKs/NVRHI)** — Multi-backend RHI API design
- **[DiligentEngine](https://github.com/DiligentGraphics/DiligentEngine)** — Cross-API abstraction patterns
- **[Piccolo](https://github.com/BoomingTech/Piccolo)** — MetaParser reflection architecture
- **[Frostbite FrameGraph](https://www.ea.com/frostbite/news/framegraph-extensible-rendering-architecture-in-frostbite)** — RenderGraph design philosophy

---

<a name="中文"></a>

# 🇨🇳 中文

## MXRender Engine

**MXRender** 是一个基于 **C++20** 和 **Vulkan** 的实时渲染引擎，核心特性包括：声明式 **RenderGraph** 及可视化节点编辑器、GPU 神经网络训练/推理（全部通过 compute shader）、**Bindless** 渲染、以及面向 Windows / Android / WebAssembly 的**多后端 RHI 抽象层**。

> **关键数据**：30+ Vulkan 后端文件 · 31 个神经网络 compute shader · 14 个可运行 Sample · 3 个 RHI 后端 · 256 级撤销/重做 · 4096 纹理 bindless 堆

### 🏗️ 架构

```
Editor.exe ──依赖──> Runtime.lib ──引用──> ThirdParty
                                          (Vulkan, GLFW, ImGui, ...)
```

严格的单向依赖（编译期强制执行）：
- **Runtime 绝不引用 Editor 头文件**
- **Render 层绝不引用 Vulkan 头文件** — 所有 GPU 操作通过 RHI 抽象层
- **UI 抽象层绝不引用后端头文件** — `UIManager.h`、`UISystem.h` 零 RmlUi/GLFW 依赖

### ✨ 核心特性

**渲染硬件抽象层 (RHI)**
- 3 个后端共存：**Vulkan**（主力，30+ 文件）· **WGPU**（实验性，14 文件）· **GLES3**（小游戏就绪，19 文件）
- 统一抽象命令：`CreateTexture` / `SetGraphicsPipeline` / `Draw` / `Dispatch` / `ResourceBarrier`
- Dynamic Rendering (Vulkan 1.3) + 传统 RenderPass 自动回退
- UE 风格 GPU 内存三级管理：DeviceMemoryManager → FragmentAllocator → ResourcePool
- Shader 反射绑定 (SPIRV-Reflect) + 管线缓存持久化 + 曲面细分 + 间接绘制

**RenderGraph 系统**
- 声明式 API：`AddRenderPass<T>()` + `builder.Read/Write(resource)`
- 编译管线：引用计数 → 裁剪 → 拓扑排序(环检测) → 时间线调度 → 分离屏障 → 瞬态资源别名
- 异步计算调度 · 黑板系统 · `.rgraph.json` 序列化 · GraphViz 导出

**可视化节点编辑器** (ImGui + ax::NodeEditor)
- 完整 Command 模式撤销/重做 (256 深度) · 事务分组 · 命令合并
- Pass 模板库 (GBuffer, Shadow, SSAO, Bloom, TAA, ToneMapping...)
- 自动保存 + 崩溃恢复 · 双层验证 (编辑器 + 运行时) · 子图折叠节点

**GPU 神经网络 (NNE)**
- 31 个 compute shader 实现全 GPU 训练：前向 → 损失 → 反向 → 权重更新
- 层类型：Linear · Conv2D · MultiHeadAttention · BatchNorm · LayerNorm · Dropout · Residual
- 激活函数：ReLU · LeakyReLU · Sigmoid · Tanh · GELU · SiLU
- 优化器：SGD · Adam · AdamW · 学习率调度器：StepLR · CosineAnnealingLR
- 3 个 Demo：MLP 螺旋分类 · CNN MNIST · Transformer

**Bindless 渲染** — 全局 descriptor set (4096 texture2D + 256 cube + 64 sampler)，带 generation 保护

**渲染技术 Sample** — 2D 稳定流体 · 3D FLIP 水 (含泡沫) · FFT 频谱海浪 · 体积云 (光线步进) · PBR Mesh

**反射与代码生成** — MetaParser (libclang) → Mustache 模板 → RTTR 注册 + JSON 序列化 + UI 绑定代码

**多后端 UI 框架** — `UIManager` 门面 + `UISystem` 抽象后端 + `RmlUISystem` 实现，零 void*，零 Rml 类型穿透

**多线程渲染** — Single / RHIThread / ThreeThread 可切换 + TaskScheduler 纤程调度

**跨平台** — Windows (Vulkan) ✅ · Android ARM64 (Vulkan) 🧪 · WASM/WebGL2 ✅ · 微信小游戏 ✅ · 抖音小游戏 ✅

### 🚀 快速开始

```batch
# 前置条件：安装 xmake 和 Visual Studio 2022/2026

# 一键生成 VS 解决方案
gen_vs_sln.bat

# 或手动构建
xmake project -k vsxmake -m "debug;release;releasedbg" -a x64 -y
xmake build Editor

# 运行 Sample
xmake build RendererSample-HelloTriangle
xmake run RendererSample-HelloTriangle
```

> **注意**：必须使用 `xmake run`，直接运行 `.exe` 会因缺少 DLL 路径而失败。

Android 交叉编译请参考上方 [Android Build](#android-build-cross-compile) 章节。

### 🎮 小游戏构建 (微信 / 抖音 / 浏览器)

**前置条件**: Emscripten SDK 6.0.4+ 安装于 `src/ThirdParty/emsdk/`。

```batch
# 一键构建全部 3 个 WASM Sample
build_wasm.bat

# 浏览器预览
cd build\wasm\wasm32\debug
python -m http.server 8080
# → http://localhost:8080/MiniGame-Mesh.html
```

**微信打包**:
```batch
pack\WeChat\build_pack.bat MiniGame-Mesh debug
# 产出: pack\WeChat\dist\MiniGame-Mesh_wx\
# → 微信开发者工具 → 导入项目 → 编译 → 预览
```

**抖音打包**:
```batch
pack\Douyin\build_pack.bat MiniGame-Mesh debug
# 产出: pack\Douyin\dist\MiniGame-Mesh_tt\
# → 抖音开发者工具 → 导入项目 → 编译 → 预览
```

**小游戏 Sample**:
| Target | 功能 |
|--------|------|
| `MiniGame-HelloTriangle` | gl_VertexID 硬编码三角形 |
| `MiniGame-Texture` | 全屏四边形 + checkerboard 纹理采样 |
| `MiniGame-Mesh` | 旋转立方体 VBO+VAO+UBO + 鼠标/触摸轨道相机 |

### 🧪 Sample 一览

| # | Target | 演示内容 |
|---|--------|---------|
| 1 | `HelloTriangle` | 最小 RenderGraph Pass · 三角形绘制 |
| 2 | `Texture` | 异步 DDS 纹理加载 + 采样 |
| 3 | `CubeMap` | DDS 立方体贴图天空盒 |
| 4 | `Reflection` | MetaParser + RTTR 反射 (控制台) |
| 5 | `Bindless` | Bindless PBR + 材质系统 |
| 6 | `NeuralNetwork` | GPU MLP 螺旋分类 |
| 7 | `NNE_MNIST_CNN` | GPU CNN 手写数字识别 |
| 8 | `NNE_Transformer` | GPU Transformer 自注意力 |
| 9 | `Fluid2D` | 2D 稳定流体 · 水墨渲染 |
| 10 | `Fluid3D` | 3D FLIP 水 · 泡沫 · 6 RDG Pass |
| 11 | `Ocean` | FFT 频谱海浪 |
| 12 | `VolumetricCloud` | 体积云 · 光线步进 · 3D 纹理 |
| 13 | `Mesh` | .obj 加载 · PBR · 间接绘制 |
| 14 | `RmlUI` | 游戏 HUD · 数据绑定 · 事件 |
| 15 | `VirtualTexture` | 虚拟纹理四叉树原型 (WIP) |

### 🗺️ 项目结构

```
src/
├── Runtime/          # 引擎核心库
│   ├── Core/         # 基础类型、ResourceHandle
│   ├── RHI/          # RHI 抽象 (Vulkan/WGPU/GLES3)
│   ├── Render/Core/  # RenderGraph 系统
│   ├── Application/  # 窗口、SampleApp 基类
	│   ├── Asset/        # MeshAsset, TextureAsset, glTF加载器, AssetPack
	│   ├── Platform/     # 平台抽象: Desktop/Android/Emscripten/WeChat/Douyin
	│   ├── Network/      # 网络抽象: KCP可靠UDP
	│   ├── Audio/        # 音频抽象: Web Audio / 微信 / 抖音
	│   ├── Input/        # InputSystem + InputActionMap
│   ├── UI/           # 多后端 UI 框架
│   └── Tool/         # ShaderLibrary、BufferUtils
├── Editor/           # 可视化 RenderGraph 编辑器
├── Sample/           # 15 个自包含 Sample
└── _Generated/       # 自动生成代码 (勿手动修改)
```

### 🤝 贡献

欢迎提交 Issue 和 PR！开发规范详见 [CLAUDE.md](CLAUDE.md)。请确保代码符合层级依赖规则。

### 📄 许可证

本项目采用 **MIT License** 授权 — 详见 [LICENSE](LICENSE) 文件。

### 🙏 致谢

本项目深受以下项目和库的启发：**Unreal Engine** (RHI 设计) · **[Nvrhi](https://github.com/GPUOpen-LibrariesAndSDKs/NVRHI)** (多后端 API) · **[DiligentEngine](https://github.com/DiligentGraphics/DiligentEngine)** (跨 API 抽象) · **[Piccolo](https://github.com/BoomingTech/Piccolo)** (反射架构) · **Frostbite FrameGraph** (RenderGraph 设计哲学)。

---

<p align="center">
  <sub>Made with ❤️ by <a href="https://github.com/1393650770">1393650770</a></sub>
</p>
