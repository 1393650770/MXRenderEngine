# MyRenderer
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![README](https://img.shields.io/badge/README-EN-red.svg)](README.md)
[![README](https://img.shields.io/badge/README-中文-blue.svg)](README_CN.md)

MyRenderer (MXRender) is a real-time rendering engine written in **C++20** on top of **Vulkan**, built with **xmake**. It features a declarative **RenderGraph** with a visual node editor, **bindless** rendering, **GPU neural network** training/inference via compute shaders, and a code-generation based **reflection** system.

## Overview

```
Editor.exe ──depends──> Runtime.lib ──includes──> ThirdParty
                                                  (vulkan, glfw, imgui, ...)
```

Strict one-way layering:
- **Runtime never includes Editor headers**
- **Render layer never includes Vulkan headers** — all GPU work goes through the RHI abstraction
- **Editor headers never include Vulkan headers** (the ImGui backend .cpp is the only exception)
- **UI layer never includes Vulkan headers** — scissor/blend/barrier all go through `RHI::CommandList`; missing RHI methods are added to the abstract interface first
- **UI abstract layer never includes backend headers** (`UIManager.h`, `UISystem.h`, `UIDataModelBinder.h`, `UIRenderer.h` are RmlUi/GLFW-free)

## How to build

### Prerequisites
- **[xmake](https://github.com/xmake-io/xmake)** (tested with v2.9.2+)
- **Visual Studio 2022/2026** with C++20 support

### Quick Start (Generate VS Solution)

Double-click `gen_vs_sln.bat` or run in cmd:
```batch
gen_vs_sln.bat
```
This script will:
1. Automatically fix gli + imgui compatibility issues
2. Clean stale caches
3. Generate `.sln` for VS (debug / release / releasedbg, x64)

> **Note for users in China:** If GitHub downloads fail, set a proxy first:
> ```batch
> set HTTP_PROXY=http://127.0.0.1:10809
> gen_vs_sln.bat
> ```

### Manual Build

##### ① Install xmake
https://github.com/xmake-io/xmake/releases

##### ② Generate VS project
```batch
xmake project -k vsxmake -m "debug;release;releasedbg" -a x64 -y
```

##### ③ Build from command line
```batch
xmake config -m debug
xmake
```

### Build notes

- `Runtime` is built as a **static lib in debug** and a **shared lib in release/releasedbg** (symbols auto-exported).
- A pre-build step (`CompileResource`) runs automatically before every build:
  1. **Reflection codegen** — `MetaParser` (libclang) scans annotated headers and generates rttr registration + JSON serializer code into `src/_Generated/` via mustache templates
  2. **Shader compilation** — `glslangValidator` compiles everything under `resource/Shader/**` to SPIR-V
  3. **FlatBuffers codegen** — `flatc` generates C++ from the `*.fbs` schemas
- After each build, shaders/textures/dlls are copied next to the executables in `build/windows/x64/<mode>/`.

### Android Build (Cross-Compile)

> **Status**: ARM64 cross-compilation + APK packaging verified. Black-screen rendering issue under investigation.

#### 前置条件

| 工具 | 用途 | 路径配置位置 |
|------|------|-------------|
| Android NDK r27+ | 交叉编译器 | `.xmake/android_ndk.txt` |
| JDK 21+ | APK 签名 | `pack/Android/build_apk.bat` 顶部 `JDK_ROOT` |
| Android SDK | build-tools / platforms / adb | `pack/Android/build_apk.bat` 顶部 `ANDROID_SDK_ROOT` |

#### 路径配置

**① NDK 路径**（xmake 编译用）

编辑 `.xmake/android_ndk.txt`，写入 NDK 根路径，一行：

```
D:/Project/AndroidNDK/android-ndk-r27d-windows/android-ndk-r27d
```

xmake 在编译阶段自动读取该文件。换机器只需改这一个文件。

**② SDK / JDK 路径**（APK 打包用）

编辑 `pack/Android/build_apk.bat`，修改顶部两个变量：

```batch
set "ANDROID_SDK_ROOT=D:\your\AndroidSDK"
set "JDK_ROOT=D:\your\AndroidSDK\jdk\jdk21"
```

build-tools、platforms、android.jar 等路径自动从 `ANDROID_SDK_ROOT` 派生。

#### 编译 & 打包

```batch
# 1. 配置并交叉编译（生成 .so）
xmake f -p android -a arm64-v8a --ndk=你的NDK路径 --ndk_sdkver=26 -y
xmake build RendererSample-HelloTriangle

# 2. 打包 APK
.\pack\Android\build_apk.bat

# 3. 安装到设备
adb install -r .\pack\Android\mxrender_hello.apk
```

#### 目录结构

```
pack/Android/
  build_apk.bat         # APK 打包脚本
  AndroidManifest.xml   # NativeActivity manifest
  debug.keystore        # 调试签名密钥（密码: android）
  mxrender_hello.apk    # 构建产物（打包后生成）
  tmp_apk/              # 临时构建目录（打包时自动创建/清理）

.xmake/
  android_ndk.txt       # NDK 根路径（一行，编译前必须配置）
```

## Branch
① The release version is in the branch [**main**] 

② Old Shit code is in branch [**batch**] , but it can run

③ Each branch develops one feature and merges into the branch [**main_dev**] 

④ The most active development branch now is [**rendergraph-editor**] (RenderGraph system + visual node editor + samples)

## Feature
###### Tip: maybe still in progress
 - [x] RHI
The architecture refers to UE
 - [x] Vulkan 
The implementation refers to UE/ Nvrhi/ DiligentEngine etc
 - [x] RenderGraph
Refers to fg
 - [x] Reflection
The architecture is metaparser(refers to Piccolo) and rttr 
 - [x] RenderGraph compile pipeline
Reference counting → culling → topological sort (cycle detection) → timeline → split barrier generation → transient resource memory aliasing; plus async compute scheduling, blackboard, GraphViz dump and `.rgraph.json` (schema v2) serialization (`src/Runtime/Render/Core/RenderGraph.cpp`)
 - [x] Visual RenderGraph node editor
ax::NodeEditor based canvas with undo/redo command system, pass templates, connection/graph validators, auto-save with crash recovery and collapsible sub-graph nodes (`src/Editor/UI/RenderGraphEditor/`)
 - [x] Bindless rendering
Descriptor-indexing global descriptor set (set 2): 4096 texture2D + 256 cube + 64 sampler slots with slot alloc/free/update (`src/Runtime/RHI/Vulkan/VK_BindlessManager.cpp`)
 - [x] GPU Neural Network (NNE)
Full training on GPU via 31 compute shaders: Linear / Conv2D / MultiHeadAttention / BatchNorm / LayerNorm / Dropout / Residual blocks, ReLU/GELU/SiLU/... activations, SoftmaxCrossEntropy loss, SGD/Adam/AdamW optimizers, LR schedulers, model save/load (`src/Sample/6-NeuralNetwork/`, `resource/Shader/nn_*.comp`)
 - [x] Multi-threaded rendering
Switchable `Single` / `RHIThread` / `ThreeThread` (Logic + Render + RHI) modes with a triple-buffered `FrameSynchronizer` (`src/Runtime/Render/Core/RenderFrameSync.h`)
 - [x] GPU memory sub-allocation
UE-style: `VK_DeviceMemoryManager` (pages) → `VK_MemoryResourceFragmentAllocator` (sub-alloc) → `VK_ResourcePool` (transient reuse by desc hash) (`src/Runtime/RHI/Vulkan/VK_Memory.cpp`)
 - [x] Dynamic Rendering
Vulkan 1.3 `vkCmdBeginRendering` path with automatic fallback to legacy render passes
 - [x] Pipeline cache persistence
`PipelineCache.bin` loaded at startup / saved at shutdown; PSOs are content-hash cached
 - [x] Shader reflection based binding
SPIRV-Reflect drives descriptor set layouts and by-name resource binding (`srb->SetResource("name", ...)`)
 - [x] Staging buffer manager
Pooled staging buffers with fence-based deferred reclamation for texture/buffer uploads
	 - [x] Multi-backend UI framework
	`UISystem` abstract backend → `RmlUISystem` (RmlUi) with pluggable architecture; `UIManager` singleton facade (holds `UISystem*`); `UIDataModelBinder` abstract data binding (zero `void*`, zero RmlUi type leaks); `UIRenderer` slim abstract renderer (3 virtual methods); `UIInputBridge` abstract input routing; RTTR-based declarative `UIWidget` framework with `UI_BIND` annotations, MetaParser codegen, and `UIWidgetManager` lifecycle
	 - [x] Tessellation support
	`PatchList` topology + hull/domain shader stages in PSO desc; `.tesc/.tese` shader files
	 - [x] Indirect draw / dispatch
	`DrawIndirect` / `DrawIndexedIndirect` / `DispatchIndirect` on CommandList with `Storage|Indirect` buffer support for GPU-written draw args
	 - [x] Storage images (UAV textures)
	`ENUM_TYPE_STORAGE` textures with `TransitionTextureState(UnorderedAccess)` / `ResourceBarrier`; `image2D`/`uimage2D` in GLSL; verified `imageAtomicMin/Add` (R32U format)
 - [ ] Virtual Texture
Quadtree page layout prototype (`src/Runtime/Render/Core/VirtualTexture/`), still WIP
 - [ ] GPU-Driven rendering
Culling/depth-reduce shaders exist (`resource/Shader/gpu_culling.comp`), C++ side currently parked in `backup/`, WIP

## Editor

The `Editor` target is an ImGui (docking + multi-viewport) application centered on a **visual RenderGraph editor**:

- **RenderGraphPanel** — node-editor canvas: add/delete pass & resource nodes, link pins, right-click menus driven by the built-in `PassRegistry` (GBuffer, DepthPre, Shadow, lighting, SSAO, Bloom, TAA, ToneMapping, ...), sync a runtime graph back into the editor
- **PropertiesPanel / OutlinePanel** — selection-synced inspection via an editor-wide `EventBus`
- **Command system** — undo/redo history with transactions and command merging
- **TemplateLibrary** — built-in GBuffer / Lighting / Shadow / PostProcess pass templates, plus user `.rgtemplate.json`
- **GraphValidator + ConnectionValidator** — pre-compile checks (cycles, connectivity, naming, pin type rules)
- **AutoSaveService** — periodic atomic auto-save with crash recovery
- **Serialization** — graphs load/save as human-readable `.rgraph.json` (schema v2); every graphics sample exports one that the editor can open

## Sample
###### Tip: Mainly to verify the engine feature

| Target | What it demonstrates |
|---|---|
| `RendererSample-HelloTriangle` | Minimal RenderGraph pass drawing a triangle; exports `hello_triangle.rgraph.json` |
| `RendererSample-Texture` | Async texture asset loading (DDS) + sampled fullscreen quad; exports `texture.rgraph.json` |
| `RendererSample-CubeMap` | DDS cubemap skybox rendering; exports `cubemap.rgraph.json` |
| `RendererSample-Reflection` | Console demo of the MetaParser + rttr reflection (property get/set, method invoke) |
| `RendererSample-Bindless` | Bindless PBR: textures allocated into the global descriptor heap, sampled by index in shader |
| `RendererSample-NeuralNetwork` | GPU-trained MLP classifying a 2D spiral dataset (forward/backward/update all in compute) |
| `RendererSample-NNE_MNIST_CNN` | MNIST-style CNN: Conv2D + BatchNorm + Dropout + AdamW, model save/load (synthetic data) |
| `RendererSample-NNE_Transformer` | Tiny Transformer: MultiHeadAttention + LayerNorm + Residual + GELU + cosine LR schedule |
| `RendererSample-Fluid2D` | Interactive 2D stable-fluids sim (mouse splat force/dye, Jacobi pressure solve) with blurred metaball ink-wash stylized rendering, all on storage buffers |
| `RendererSample-Fluid3D` | GPU FLIP water: MAC-grid pressure projection, fixed-point-atomic P2G, GPU free-list particle recycling, screen-space water rendering with foam; 6 RDG passes, retained storage-image textures with manual layout transitions |
| `RendererSample-Ocean` | FFT spectral waves: compute IFFT chain, storage-buffer export to graphics PSO, vertex-pulling grid |
| `RendererSample-VolumetricCloud` | 3D-texture reference: Hillaire sky LUTs + Nubis-style raymarched clouds; one-shot compute bake of tileable 3D Perlin-Worley noise into `image3D`, per-frame sky-view LUT + half-res cloud march, `sampler3D` reads in compute and fragment stages |
| `RendererSample-Mesh` | Vertex-input reference: MeshAsset obj load → `Vertex|Dynamic` VB/IB, `vertex_input_layout` PSO, OrbitCameraController + SceneView, DrawIndexed vs DrawIndexedIndirect toggle |
| `RendererSample-RmlUI` | RmlUI game UI demo: HP bar, score, timer with `UI_BIND` annotations; multi-backend `UISystem` architecture; `UIDataModelBinder` abstract binding (zero `void*`, zero Rml types in Sample layer) |
| `RendererSample-VirtualTexture` | Virtual texture quadtree page-layout prototype |

Run any sample from its output directory (shaders are loaded via relative `Shader/...` paths), e.g.:
```batch
xmake build RendererSample-Fluid2D
xmake run RendererSample-Fluid2D
```

## Project Structure

```
src/
  Runtime/              # Engine library
    Core/               # Base types & macros (ConstDefine.h), reflection runtime
    RHI/                # Render Hardware Interface (platform-agnostic)
      Vulkan/           # Vulkan backend (VK_* classes)
    Render/Core/        # RenderGraph system (+ serializer, validator, pass registry)
    Application/        # Window / main loop (GLFW)
    Asset/              # Mesh / texture / material assets
    Platform/           # Platform abstractions
	    UI/                 # Multi-backend UI framework (UISystem, UIManager, RmlUI backend)
	      Widget/           #   RTTR-based declarative widget framework (UIWidgetManager)
	      RmlUI/            #   RmlUI concrete backend (RmlUISystem, RmlUIRenderer, RmlDataModelBinder)
    GenCode/            # Generated code (flatbuffers schemas, embedded SPIR-V)
  Editor/               # Editor application
    EditorRender/       # EditorRenderPipeline + EditorUI (ImGui)
    UI/                 # Node/Pin/Link/Panel framework + RenderGraphEditor
  Sample/               # RendererSample-* sources
  Reflect/meta_parser/  # MetaParser (libclang based codegen tool)
  _Generated/           # Reflection & serializer generated code
resource/               # Shaders (GLSL + SPIR-V), textures (incl. Sponza), editor assets
template/               # Mustache templates for reflection codegen
```

## Third-Party

Fetched via xmake packages: `vulkansdk`, `glfw 3.4`, `glm`, `imgui 1.89.9-docking`, `assimp`, `tinyobjloader`, `gli`, `lz4`, `nlohmann_json`, `rttr`, `boost 1.84`, `flatbuffers 1.12`, `glslang`.

Vendored in-tree: `RmlUi` (HTML/CSS UI middleware), `TaskScheduler` (multithreaded task scheduler), `SPIRV-Reflect`, `stb_image`, `VulkanMemoryAllocator`, `imgui-node-editor` (ax::NodeEditor), `libclang` (for MetaParser).
