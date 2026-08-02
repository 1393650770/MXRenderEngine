# CLAUDE.md

-- Code development must prioritize extensibility and architecture. Always follow design patterns!!! Always follow design patterns!!! Avoid tight coupling.

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

MyRenderer (MXRender) is a real-time Vulkan-based rendering engine written in C++20. It features a declarative RenderGraph system with a visual node editor, GPU neural network inference, virtual texturing, and bindless rendering.

## Architecture

```
src/
  Runtime/              # Core engine library (single monolithic lib)
    Core/               # Base types, macros (ConstDefine.h), reflection
    RHI/                # Render Hardware Interface — platform-agnostic
      Vulkan/           # Vulkan backend (VK_* prefix)
    Render/Core/        # RenderGraph system
    Application/        # Window management (GLFW)
    Asset/              # Asset loading (mesh, texture, material)
    Platform/           # Platform abstractions
    UI/                 # UI framework — multi-backend (RmlUI, future ImGui)
      UISystem.h        #   Abstract backend base (virtual, not pure virtual)
      UIDataModelBinder.h # Abstract data binding (zero void*, zero RmlUi)
      UIManager.h       #   Singleton facade (holds UISystem*)
      UIRenderer.h      #   Abstract renderer (3 virtual methods)
      UIInputBridge.h   #   Abstract input bridge
      RmlUI/            #   RmlUI concrete backend
      Widget/           #   RTTR-based declarative widget framework
    GenCode/            # Generated code (flatbuffers, shader SPIR-V)
  Editor/               # Editor application
    Editor.cpp          # Entry point
    EditorRender/       # EditorRenderPipeline + EditorUI (ImGui)
    UI/
      BaseNode/Pin/Link/Panel/Item  # Generic node-graph editor base
      RenderGraphEditor/ # Visual RenderGraph node editor
        Services/       # Serializer, Builder, Validator, PassRegistry, EventBus
        Commands/       # Undo/Redo command system
        Templates/      # Pass template library
  Sample/               # RendererSample-* demos (one dir per sample, see Samples section)
  Reflect/meta_parser/  # MetaParser codegen tool (libclang)
  _Generated/           # Generated reflection/serializer code (do not hand-edit)
```

## Layer Dependency (Strict One-Way)

```
Editor.exe  ──depends──>  Runtime.lib  ──includes──>  ThirdParty/
                                                   (vulkan, glfw, imgui, etc.)
```

- **Runtime NEVER includes Editor headers** (src/Editor/ not in Runtime include path)
- **Render layer NEVER includes Vulkan headers** (all VK ops through RHI abstraction)
- **Editor headers NEVER include Vulkan headers** (EditorUI.cpp is the only exception, for ImGui backend)
- **UI layer NEVER includes Vulkan headers** (all GPU ops through RHI::CommandList; scissor/blend/etc. must be on the RHI abstract interface, not via VK_* casts)
- **UI abstract layer NEVER includes RmlUi/GLFW headers** (`UIManager.h`, `UISystem.h`, `UIDataModelBinder.h`, `UIRenderer.h` are backend-agnostic)

## Build System

- **xmake** (v2.9.2+), generate VS solution: `gen_vs_sln.bat`
- Build: `xmake build Editor` (or any `RendererSample-*` target)
- Run: `xmake run <target>` — required; launching the exe directly fails to find glfw3.dll (shared libs live in the xmake package cache and `xmake run` injects the PATH). Working directory must be the output dir (`build/windows/x64/<mode>/`) because shaders load via relative `Shader/...` paths — `xmake run` handles this.
- `Runtime` is a static lib in debug, shared lib in release/releasedbg
- Pre-build (`CompileResource` target) runs automatically: ① MetaParser (libclang + mustache) generates reflection/serializer code into `src/_Generated/` ② `glslangValidator` compiles `resource/Shader/**` to `.spv` ③ `flatc` generates flatbuffers C++
- Post-build `MoveResource` copies `.spv` (flattened!), textures, and dlls into the output dir
- Key dependencies: vulkansdk, glfw, glm, imgui (docking), nlohmann_json, flatbuffers, boost, assimp
- C++20, UTF-8 (no BOM) encoding for .cpp/.h files

## Code Conventions

- **Dual-track style (规范 V2)**:
  - Classes that need reflection output (serialization / network replication / UI binding / runtime type name) → `MYRENDERER_BEGIN_CLASS(Name)` + field `META(...)` (reflection is opt-in; MetaParser silently skips unmarked classes)
  - Everything else (engine classes, pass data structs, Editor commands, pure logic) → modern C++ direct: `class X { virtual ~X() = default; void Run() override; }`. Class-internal methods/fields are ALWAYS modern-style, even in reflected classes.
  - Precedent: `src/Editor/UI/RenderGraphEditor/Commands/` is fully modern-style.
- **DEPRECATED (new code must not use): `VIRTUAL` / `METHOD()` / `OVERRIDE` / `CONST` / `MYDEFAULT` / `MYDELETE` / `PURE`** — they are keyword synonyms; `__cdecl` (x64 default) adds nothing. ~3000 legacy sites stay untouched (migrate on touch). Known debt: METHOD/VIRTUAL legacy macros.
- **Assertions**: new code uses positive-semantics `ENSURE(cond, ...)` (fires when cond is FALSE). The `CHECK` family keeps its INVERTED semantics for existing callers — `CHECK(flag)` fires when flag is TRUE.
- Type aliases: `String`, `Vector`, `Map`, `Bool`, `UInt32`, `Int`, etc. (from ConstDefine.h)
- Namespaces: `MXRender::Render`, `MXRender::RHI`, `MXRender::UI`, `MXRender::RHI::Vulkan`, `MXRender::Core` (Job), `MXRender::Gameplay`, `MXRender::World`
- `#pragma region METHOD` / `#pragma region MEMBER` for class organization
- **UTF-8 (no BOM) encoding for all .cpp/.h files**; Chinese comments allowed (the old "GBK / keep comments ASCII" rule was wrong — the repo is ~99% UTF-8)
- **`CHECK(flag)` / `CHECK_WITH_LOG(flag, msg)` fire when the condition is TRUE** — inverted vs standard assert (e.g. `CHECK_WITH_LOG(vkCreate...(...) != VK_SUCCESS, "...")`)
- Detailed conventions (namespace layout, macro system, file templates) live in the `myrenderer-conventions` skill

## Cross-Thread Command Channel (ENQUEUE_RENDER_COMMAND)

Logic thread → render thread one-shot operations (`src/Runtime/Render/Core/CommandQueue.h`, header-only):

```cpp
ENQUEUE_RENDER_COMMAND(CreateMesh)([p = std::move(payload)]{ ... });   // fire-and-forget (statement-only)
struct MyCmdTag {};                                                    // fence variant needs a tag type
RenderCommandFence fence = EnqueueRenderCommand<MyCmdTag>("MyCmd", [=]{ ... });
```

- **Snapshot vs command rule**: data the render thread reads EVERY frame (camera/player params/HUD) → FrameContext snapshot; one-shot lifecycle ops (create/destroy resource, one-time upload, editor action) → command queue. >5-10 commands/frame = design error.
- **Fence semantics**: `IsComplete()` proves CPU execution on the render thread only, NOT GPU completion. The only safe GPU visibility model is "fence complete + next frame RDG reference". Never rewrite captured mapped memory right after Wait().
- **Deadlock rule**: `fence.Wait()` only AFTER `SignalFrameReady()`. Command bodies must never call FrameSynchronizer logic APIs or `Begin()/End()/SetBypass()/SwapCommandLists()`.
- **Record-type commands** (bodies calling `RHIGetWriteCommandList()`) are UB in bypass mode (no render thread) — assert via `RenderCommandFlags::Record`.
- Thread assertions: `ASSERT_RENDER_THREAD()` / `ASSERT_LOGIC_THREAD()`.
- Flush point: `RenderFrameSync.cpp::RenderThreadMain` after `Begin()`, before `OnPreRender`. Shutdown sequence: join → `Drain()` (before `OnShutdown_Render`) → `Shutdown()`.

## JobSystem & TaskGraph (`src/Runtime/Core/Job/`)

- Worker pool + dependency DAG for the logical tick; 0-worker mode = fully sequential (Android/Web). Lifecycle owned by GameWorld (Initialize in ctor, Shutdown in dtor).
- `Execute()` is called by the logic thread which PARTICIPATES until completion. Workers NEVER block on graph completion (no WaitAll inside task bodies — that starves).
- Task args may be heap-owned: pass an `arg_dtor` to `TaskGraph::AddTask` (JobSystem calls it after fn).
- Per-tick graphs are rebuilt via `TaskGraph::Reset()` (node pool keeps pointers stable — std::deque, NOT std::vector).
- Parallel ECS iteration (`EnntECSSystem::ParallelForEach`): cache storage references OUTSIDE the entity loop — EnTT's `registry.get/all_of` (assure → dense_map) cost ~23µs/call in this build.

## Samples (src/Sample/)

Each sample is a single self-contained cpp with `main()`: a class deriving `MXRender::RenderInterface` overriding `OnInit_Logic / OnShutdown_Logic / OnUpdate / OnRender`, driven by `Window::Run`. `2-Texture/Texture.cpp` is the minimal RenderGraph template; `6-NeuralNetwork/` is the compute-shader reference (`ShaderHelper.h` for compute PSO creation); `7-Fluid2D/` combines both (compute sim + fullscreen draw in one RDG pass); `8-Fluid3D/` is the heavy-compute reference (GPU FLIP water: MAC-grid pressure projection, fixed-point-atomic P2G, GPU free-list particle recycling, screen-space water rendering with foam; 6 RDG passes, retained storage-image textures with manual layout transitions); `9-Ocean/` is FFT spectral waves (compute IFFT chain, storage-buffer export to the graphics PSO, vertex-pulling grid); `10-VolumetricCloud/` is the 3D-texture reference (Hillaire sky LUTs + Nubis-style raymarched clouds: one-shot compute bake of tileable 3D Perlin-Worley noise into `image3D`, per-frame sky-view LUT + half-res cloud march, `sampler3D` reads in compute and fragment stages); `11-Mesh/` is the vertex-input reference (MeshAsset obj load → `Vertex|Dynamic` VB/IB, `vertex_input_layout` PSO, OrbitCameraController + SceneView, DrawIndexed vs DrawIndexedIndirect toggle via I key or `MESHSAMPLE_INDIRECT=1`, built on the SampleApp base).

New samples should derive `Application::SampleApp` (`src/Runtime/Application/SampleApp.h`) instead of raw `RenderInterface`: it provides the whole `main()` (`SampleApp::RunSample(app, title)`), BackBuffer/DepthStencil retained-resource registration (`GetBackBufferResource()`), `BindBackBufferTarget(cmd)` (SetRenderTarget + clear values), auto `graph.Compile()`, and `SaveGraphDefinition(name, path)`. Subclass hook: `OnInitScene()` (required) / `OnShutdownScene()` — release all GPU resources you own in `OnShutdownScene`; member destruction at end of main() runs AFTER RHIShutdown and trips the buffer-leak assert (use e.g. `MeshAsset::ReleaseBuffers()`). Runtime common utilities (collected from the former per-sample copies): `Tool::ShaderLibrary` (ReadSpirv/LoadShader/CreateComputePSO), `Tool::BufferUtils` (CreateStorageBuffer/CreateDynamicParamBuffer/Upload), `Tool::ComputeUtils::DispatchWithBarrier` (dispatch + the UAV→SRV/UAV→UAV barrier pair), `Render::SceneView` (view/proj matrix set with the unified Vulkan conventions: GLM 0..1 depth + Y-flip baked into proj; include it before any other glm header) + `Application::OrbitCameraController` (LMB rotate / scroll zoom / MMB pan; `Attach()` owns the GLFW scroll callback — don't mix with a manual `glfwSetScrollCallback`).

Adding a sample:
1. New `src/Sample/X-Name/Name.cpp` (with `main`)
2. Copy the 5-line target block in `xmake.lua` (`CommonProjectSetting()` + `add_files` + `set_group("Sample")` + `after_build(MoveResource)`)
3. Shaders go in `resource/Shader/Sample/` — compiled and copied automatically, but the copy is **flattened** into `Shader/`, so prefix filenames uniquely (e.g. `fluid_*`); load at runtime as `"Shader/<name>.spv"`

There is no input system: poll GLFW directly (`glfwGetMouseButton` / `glfwGetCursorPos` on `window->GetWindow()`) in `OnUpdate`. `Window.h` pulls in windows.h via GLFW/Vulkan — its min/max macros break `std::min/max`.

## RHI Gotchas (verified in practice)

- **Storage images (UAV textures) are supported**: create with `ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_STORAGE` (add `ENUM_TYPE_SHADERRESOURCE` to also sample it), bind GLSL `image2D/uimage2D` via `SetResource` (descriptor written with GENERAL layout). `imageAtomicMin/Add` need format `R32U` (`r32ui` in GLSL). Keep storage textures `mip_level = 1`. Transition with `TransitionTextureState(tex, UnorderedAccess)` before compute writes and `ShaderResource` before sampling (RDG does this automatically for transient resources declared with `Read/Write(res, state)`).
- **Compute PSOs**: use `RenderGraphiPipelineStateDesc` with only `shaders[Shader_Compute]` filled + `RHICreateRenderPipelineState` (the separate `RHICreateComputePipelineState`/`ComputePipelineState` path has no CommandList bind entry point).
- **PSO cache hashes shader pointers**: create ALL shaders first, then all PSOs, then delete the shaders — interleaving create/delete can recycle a heap address and silently return the wrong cached PSO.
- **SRB binding is by GLSL instance name** (spirv_reflect); unknown names throw. `CreateShaderResourceBinding(srb, true)` is static/bind-once semantics (later `SetResource` silently ignored) — use `false` and bind once at init for ping-pong setups. **Never call `SetResource` at runtime (per frame)**: descriptor sets are persistent and the previous frame's command buffer may still be executing (see thread mode below) — `vkUpdateDescriptorSets` on an in-flight set is undefined behavior (manifests as resources binding to the wrong frame, e.g. clears landing on the wrong texture). ALL bindings must be complete at init; this also means transient RDG textures cannot be bound through SRBs (their actual changes every frame) — use retained textures with manual `TransitionTextureState` instead until per-frame descriptor sets exist.
- **Shader param buffers must be `readonly buffer` (storage), not `uniform` blocks** — `ENUM_BUFFER_TYPE::Storage` buffers lack UNIFORM usage.
- **Barriers**: use `cmd->ResourceBarrier(src_state, dst_state)`; `MemoryBarrier(ENUM_SHADER_STAGE, ...)` misuses shader-stage bits as pipeline-stage bits (known bug). RDG auto-barriers only apply to **transient** resources at pass boundaries (per-pass state from `Read/Write(res, state)`) — retained resources, buffers, and intra-pass hazards need manual barriers. Same-state UAV→UAV across passes emits nothing (`TransitionTextureState` early-outs on equal state) — use the Fluid2D double `ResourceBarrier` after each Dispatch.
- **Frame sync / threading**: the actual thread mode is **ThreeThread** — `Platform.cpp` (Win) hardcodes `factory.threading_mode = ThreeThread` and `RenderRHI.h`'s factory default is the same; the `g_thread_mode = Single` in ConstGlobals.cpp is overwritten during RHIInit. Two frame command buffers alternate (`write_cb`/`rhi_cb`), each with its own fence, and `Begin()` only waits its OWN fence (= frame N-2), so **two frames can be in flight**: frame N's CPU work can overlap frame N-1's GPU execution. Per-frame `RHIMapBuffer` upload of param buffers inside an execute lambda is the established pattern (Fluid2D/Fluid3D) and tolerable, but is not strictly race-free; per-frame descriptor updates are NOT safe (see SRB bullet). The swapchain prefers MAILBOX whenever available (ignores the vsync flag), which makes frame overlap chronic.
- **CPU buffer uploads**: `RHIMapBuffer(Write)` on a plain Storage buffer goes through a staging buffer + TRANSFER-queue copy with no cross-queue sync — **unreliable both per-frame and at init** (data may never become visible to the graphics queue). For per-frame params, create the buffer as `Storage | Dynamic`: it allocates host-visible persistently-mapped memory and Map/Unmap becomes a direct write (Fluid3D `fp_buf` pattern). For large init data (particle prefill etc.), write it with a one-shot compute shader instead of uploading (Fluid3D `fluid3d_prefill.comp` pattern). **Vertex/index buffers follow the same rule**: create as `Vertex|Dynamic` / `Index|Dynamic` + `Map/memcpy/Unmap` (the `MeshAsset` pattern, 2026-07) — a plain `Vertex` buffer is device-local and its Map silently takes the unreliable staging path.
- **Buffer usage bits combine** (2026-07): both translation functions (`Translate_Buffer_usage_type_To_VulkanUsageFlag`, `TranslateBufferTypeToVulkanAllocationFlags`) are bitwise — `Storage|Indirect` (compute-written draw args), `Vertex|Dynamic`, `Uniform|Dynamic` etc. all work. Host visibility is expressed solely by the `Staging`/`Dynamic` bits.
- **Indirect draws exist** (2026-07): `DrawIndirect / DrawIndexedIndirect / DispatchIndirect` on CommandList (args structs `DrawIndirectArgs` etc. in RenderCommandList.h match the VK layout; the VK impl adds the pooled sub-allocation `GetOffset()`). For GPU-written args use a `Storage|Indirect` buffer and a manual `ResourceBarrier(UnorderedAccess, IndirectArgument)` between the producing pass and the draw. Verified in `11-Mesh`.
- **Tessellation is wired** (2026-07): set `primitive_topology = PatchList` + `patch_control_points` on the PSO desc (0 treated as 3) and fill `shaders[Shader_Hull/Shader_Domain]`; `pTessellationState` is emitted automatically. `.tesc/.tese` files in resource/Shader compile like any other stage. RT shader stages (`Shader_RayGen..Shader_Callable`, values 16+) are enum+translation placeholders only — no RT pipeline/AS/SBT yet.
- **After changing struct layout or vtables in widely-included headers** (e.g. adding a PSO desc field), run `xmake build -r <target>` before any runtime test: incremental builds can link stale objects with the old ABI, crashing with 0xC0000005 in unrelated samples (looks exactly like a real bug; cost hours on 2026-07-19).
- **GPU overload no longer corrupts**: `VK_CommandBuffer::Begin` retries the fence wait until signaled (previously a 1s timeout was ignored and the in-flight command buffer was reset — permanent GPU hang under heavy per-frame workloads, e.g. 100+ dispatches).
- **Resource pool is LIFO** (`VK_ResourcePool`): multiple transient resources with identical descs swap physical identities every frame (returned then re-acquired in reverse order). Combined with the SRB constraint above this is why per-frame rebinding of transients corrupts frames.
- **32-bit float formats (R32F/RG32F/RGBA32F) do not guarantee linear filtering** in Vulkan (NV/AMD don't support it) — use RGBA16F for anything sampled with a linear sampler.
- Inside one RDG pass execute lambda, "N × Dispatch then SetRenderTarget + Draw" is valid: Dispatch auto-ends the render pass and both Dispatch and SetRenderTarget flush pending barriers first.
- Formats missing from `Translate_Texture_Format_To_Vulkan` throw at texture creation; `R32U/R32I` and 3D image type were added 2026-07, but many formats (R16 family int, ASTC, etc.) still fall through. `GetTextureFormatAttribs` has its own separate table (`RenderTexture.cpp`) — a format used with `TransitionTextureState` must exist in BOTH. **R16F/RG16F are in the VK translation but missing from `GetTextureFormatAttribs`** — creating works, first transition throws; use RGBA16F instead (verified). **3D textures are verified in practice** (`10-VolumetricCloud`): `ENUM_TYPE_3D` + `desc.depth`, `image3D` storage writes, `sampler3D` reads in compute and fragment stages all work.
- Every texture carries a built-in sampler: LINEAR min/mag + **CLAMP_TO_EDGE on all axes** (`VK_Utils::Create_Linear_Sampler`); there is no SamplerDesc/REPEAT option — tiling noise textures must wrap coordinates with `fract()` in the shader.

## RenderGraph Architecture

### Runtime (src/Runtime/Render/Core/)

- **RenderGraph**: Pass/resource container, Compile() → Execute() pipeline
  - `Compile()`: Reference counting → Culling → Topo sort → Timeline → Barrier generation → Aliasing
  - `Execute()`: Realize → Prologue barriers → Pass execute → Epilogue barriers → Derealize
  - Transient resources (`builder.Create` + `Read/Write(res, state)`): realized from the cross-frame pool (`VK_ResourcePool`), returned to it on Derealize; passes with no declared resources have ref_count 0 and MUST call `SetIsCullable(false)` or they are dropped from the timeline. **Caveat**: transient textures cannot be bound through SRBs (per-frame `SetResource` races with in-flight frames, see RHI Gotchas) — until per-frame descriptor sets exist, prefer retained textures + manual `TransitionTextureState` (`8-Fluid3D` pattern).
- **RenderGraphPass<T>**: Template pass with typed data, setup lambda (declares deps), execute lambda (records commands)
- **RenderGraphResource<Desc, Actual>**: Template resource with descriptor and actual RHI object
- **RenderGraphDefinition**: Pure-data IR for serialization (JSON)
- **RenderGraphCompileConfig**: Compile toggles + safe mode

### Editor (src/Editor/UI/RenderGraphEditor/)

- **RenderGraphPanel**: Main ImGui node-editor canvas (ax::NodeEditor)
- **GraphValidator**: Pre-compile validation (cycles, connectivity, naming)
- **RenderGraphBuilder**: Definition → Runtime graph construction (Editor-side)
- **RenderGraphSerializer**: JSON save/load (.rgraph.json, v2 schema)
- **CommandHistory**: Undo/Redo (max 256 depth, command merging)
- **EditorEventBus**: Observer pattern for panel communication
- **TemplateLibrary**: Built-in pass templates (GBuffer, Lighting, Shadow, PostProcess)

## Key Files Reference

| Purpose | Path |
|---------|------|
| Type aliases, macros | `src/Runtime/Core/ConstDefine.h` |
| RHI enums (states, formats) | `src/Runtime/RHI/RenderEnum.h` |
| RHI resource descriptors | `src/Runtime/RHI/RenderRource.h` |
| RenderGraph core | `src/Runtime/Render/Core/RenderGraph.h` |
| RenderGraphDefinition (IR) | `src/Runtime/Render/Core/RenderGraphDefinition.h` |
| RenderGraphResource + barriers | `src/Runtime/Render/Core/RenderGraphResource.h` |
| VK resource pool + bridge | `src/Runtime/RHI/Vulkan/VK_ResourcePool.cpp` |
| Sample base class | `src/Runtime/Application/SampleApp.h` |
| Camera (view math / controller) | `src/Runtime/Render/View/SceneView.h`, `src/Runtime/Application/CameraController.h` |
| Mesh loading | `src/Runtime/Tool/MeshLoader.h`, `src/Runtime/Asset/MeshAsset.h` |
| Shader/buffer/compute helpers | `src/Runtime/Tool/ShaderLibrary.h`, `BufferUtils.h`, `ComputeUtils.h` |
| Editor main panel | `src/Editor/UI/RenderGraphEditor/RenderGraphPanel.cpp` |
| Graph validator | `src/Editor/UI/RenderGraphEditor/Services/GraphValidator.h` |
| Command system | `src/Editor/UI/RenderGraphEditor/Commands/CommandHistory.h` |
| Template library | `src/Editor/UI/RenderGraphEditor/Templates/TemplateLibrary.h` |
| UI abstract backend | `src/Runtime/UI/UISystem.h` |
| UI abstract binder | `src/Runtime/UI/UIDataModelBinder.h` |
| UI abstract renderer | `src/Runtime/UI/UIRenderer.h` |
| UIManager (facade) | `src/Runtime/UI/UIManager.h` |
| RmlUI backend | `src/Runtime/UI/RmlUI/RmlUISystem.h` |
| RmlUI renderer | `src/Runtime/UI/RmlUI/RmlUIRenderer.h` |
| RmlUI data binder | `src/Runtime/UI/RmlUI/RmlDataModelBinder.h` |
| UI binding traits | `src/Runtime/UI/Widget/UIWidgetBinding.h` |

## Layering Verification Checklist

After each significant change, verify:
```bash
# No Vulkan includes in Render layer
grep -r '#include.*vulkan' src/Runtime/Render/ | wc -l  # must be 0
# No Editor includes in Runtime  
grep -r '#include.*Editor' src/Runtime/ | wc -l         # must be 0
# No VK_ includes in Render layer
grep -r '#include.*VK_' src/Runtime/Render/ | wc -l     # must be 0
# No VK_ includes in UI layer
grep -r '#include.*RHI/Vulkan' src/Runtime/UI/ | wc -l  # must be 0
# No RmlUi includes in UI abstract layer
grep -r '#include.*<RmlUi' src/Runtime/UI/UISystem.h src/Runtime/UI/UIDataModelBinder.h src/Runtime/UI/UIManager.h src/Runtime/UI/UIManager.cpp src/Runtime/UI/UIRenderer.h | wc -l  # must be 0
# No GLFW includes in UI layer (EditorUI.cpp is the only exception)
grep -r '#include.*GLFW\|glfwGetTime' src/Runtime/UI/ | wc -l  # must be 0
```

## Android Cross-Compile & APK Build

### Platform Architecture

```
Platform/                        # Cross-platform abstraction layer
  PlatformWindow.h               # Abstract window + input interface
  Desktop/
    DesktopWindow.h/.cpp         # GLFW implementation (Windows/Linux)
  Android/
    AndroidWindow.h/.cpp         # ANativeWindow implementation
    AndroidLaunch.cpp            # android_main() entry → calls Sample's main()
    Platform.cpp                 # Android RHI factory

Input/                           # Cross-platform input system
  InputKeys.h/.cpp               # Key definitions + EKey namespace
  InputSystem.h/.cpp             # Input accumulator (poling + event)
```

### Key principles
- `PlatformWindow` is the abstract window base — all Sample code uses only this interface
- `CreatePlatformWindow(title,w,h,platform_data)` — factory, desktop gets GLFW, Android gets ANativeWindow
- `SampleApp::SetPlatformData(void*)` — called once by Android entry, passed through to Window
- `InputSystem::Get()` — singleton input state, replaces direct GLFW polling
- All Android-specific code is guarded by `#if PLATFORM_ANDROID`
- Desktop files excluded from Android build via `remove_files()` in `CommonLibrarySetting`

### NDK / SDK Path Config
- NDK path: `.xmake/android_ndk.txt` (one line)
- SDK build-tools / platform / JDK paths: `.xmake/apk/build_apk.bat` (set variables at top)

### Common Android Porting Patterns
- `#include <GLFW/glfw3.h>` → wrap with `#if !PLATFORM_ANDROID`
- `<xutility>`, `__declspec`, `_byteswap_uint64`, `UINT` → MSVC-only, need `#ifdef _MSC_VER`
- `std::max`/`std::min` → need `#include <algorithm>` + `using std::max/min` on Clang
- `<boost/stacktrace.hpp>` → wrapped, CHECK macros simplified for Android
- `<ucontext.h>` (MTFiber) → disabled on Android, stub Fiber class provided
- `assimp`, `MeshLoader`, `MeshAsset` → excluded from Android build
- `vkCmdBeginRendering`/`vkCmdEndRendering` → wrapped with `#if !PLATFORM_ANDROID` in VK_CommandBuffer
- ImGui UI functions (BeginUI/EndUI) → stubbed on Android
- `add_syslinks("vulkan")` on Android → need `--unresolved-symbols=ignore-in-shared-libs` linker flag
