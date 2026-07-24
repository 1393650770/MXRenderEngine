# MyRenderer
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Language](https://img.shields.io/badge/language-C++-blue.svg)](https://isocpp.org/)
[![README](https://img.shields.io/badge/README-EN-red.svg)](README.md)
[![README](https://img.shields.io/badge/README-中文-blue.svg)](README_CN.md)

MyRenderer (MXRender) 是一个基于 **Vulkan** 的实时渲染引擎，使用 **C++20** 编写，**xmake** 构建。核心特性包括：声明式 **RenderGraph** 及其可视化节点编辑器、**Bindless** 渲染、GPU 神经网络训练/推理、以及代码生成驱动的 **反射** 系统。

---

## 架构概览

```
Editor.exe ──依赖──> Runtime.lib ──引用──> ThirdParty
                                           (vulkan, glfw, imgui, ...)
```

严格的单向依赖：

- **Runtime 绝不引用 Editor 头文件**
- **Render 层绝不引用 Vulkan 头文件** — 所有 GPU 操作通过 RHI 抽象层完成
- **Editor 头文件绝不引用 Vulkan 头文件**（EditorUI.cpp 是唯一的例外，用于 ImGui 后端）
- **UI 层绝不引用 Vulkan 头文件** — 裁剪/混合/屏障全部通过 `RHI::CommandList`；若 RHI 缺少方法，先在抽象接口层补齐
- **UI 抽象层绝不引用后端头文件**（`UIManager.h`、`UISystem.h`、`UIDataModelBinder.h`、`UIRenderer.h` 零 RmlUi/GLFW 依赖）

---

## 构建指南

### 环境要求

- **[xmake](https://github.com/xmake-io/xmake)**（推荐 v2.9.2+）
- **Visual Studio 2022/2026**，支持 C++20

### 快速开始（生成 VS 解决方案）

双击 `gen_vs_sln.bat` 或在命令行中执行：

```batch
gen_vs_sln.bat
```

脚本会自动完成以下步骤：
1. 修复 gli 与 imgui 的兼容性问题
2. 清理过期缓存
3. 生成 `.sln`（debug / release / releasedbg，x64）

> **国内用户提示：** 如果 GitHub 下载失败，请先设置代理：
> ```batch
> set HTTP_PROXY=http://127.0.0.1:10809
> gen_vs_sln.bat
> ```

### 手动构建

##### ① 安装 xmake

https://github.com/xmake-io/xmake/releases

##### ② 生成 VS 工程

```batch
xmake project -k vsxmake -m "debug;release;releasedbg" -a x64 -y
```

##### ③ 命令行编译

```batch
xmake config -m debug
xmake
```

### 构建说明

- `Runtime`：**debug 模式为静态库**，release/releasedbg 模式为动态库（符号自动导出）。
- 每次构建前自动执行预构建步骤（`CompileResource`）：
  1. **反射代码生成** — `MetaParser`（libclang）扫描注解头文件，通过 Mustache 模板生成 rttr 注册 + JSON 序列化代码到 `src/_Generated/`
  2. **Shader 编译** — `glslangValidator` 将 `resource/Shader/**` 下所有 shader 编译为 SPIR-V
  3. **FlatBuffers 代码生成** — `flatc` 从 `*.fbs` 生成 C++ 代码
- 构建完成后，shader/纹理/dll 自动复制到 `build/windows/x64/<mode>/` 目录下。

---

## Android 交叉编译

> **状态**：ARM64 交叉编译 + APK 打包已验证通过，黑屏渲染问题排查中。

### 前置条件

| 工具 | 用途 | 路径配置位置 |
|------|------|-------------|
| Android NDK r27+ | 交叉编译器 | `.xmake/android_ndk.txt` |
| JDK 21+ | APK 签名 | `pack/Android/build_apk.bat` 顶部 `JDK_ROOT` |
| Android SDK | build-tools / platforms / adb | `pack/Android/build_apk.bat` 顶部 `ANDROID_SDK_ROOT` |

### 路径配置

**① NDK 路径**（xmake 编译用）

编辑 `.xmake/android_ndk.txt`，写入 NDK 根路径，仅一行：

```
D:/Project/AndroidNDK/android-ndk-r27d-windows/android-ndk-r27d
```

xmake 编译阶段自动读取该文件，换机器只需修改此文件。

**② SDK / JDK 路径**（APK 打包用）

编辑 `pack/Android/build_apk.bat`，修改顶部两个变量：

```batch
set "ANDROID_SDK_ROOT=D:\your\AndroidSDK"
set "JDK_ROOT=D:\your\AndroidSDK\jdk\jdk21"
```

build-tools、platforms、android.jar 等路径自动派生。

### 编译 & 打包

```batch
# 1. 配置并交叉编译（生成 .so）
xmake f -p android -a arm64-v8a --ndk=你的NDK路径 --ndk_sdkver=26 -y
xmake build RendererSample-HelloTriangle

# 2. 打包 APK
.\pack\Android\build_apk.bat

# 3. 安装到设备
adb install -r .\pack\Android\mxrender_hello.apk
```

### 目录结构

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

---

## 分支策略

1. 发布版本在 [**main**] 分支
2. 早期遗留代码在 [**batch**] 分支（可运行，但架构已过时）
3. 各功能分支开发完成后合并到 [**main_dev**]
4. 当前最活跃的开发分支是 [**rendergraph-editor**]（RenderGraph 系统 + 可视化节点编辑器 + Sample）

---

## 功能特性

> 标注 `[ ]` 的项仍在开发中

### 核心架构

- **[x] RHI（渲染硬件抽象层）**
  参考 UE 架构设计，提供平台无关的 GPU 接口；Vulkan 后端参考 UE/Nvrhi/DiligentEngine 实现

- **[x] RenderGraph（渲染图系统）**
  基于 fg 的声明式渲染图：引用计数 → 裁剪 → 拓扑排序（含环检测）→ 时间线 → 分离屏障生成 → 瞬态资源内存别名；支持异步计算调度、黑板系统、GraphViz 导出、`.rgraph.json`（v2 模式）序列化

- **[x] 反射系统**
  MetaParser（参考 Piccolo）+ rttr 双引擎：libclang 扫描注解 → Mustache 模板生成 rttr 注册代码 + JSON 序列化器

- **[x] 多线程渲染架构**
  `Single` / `RHIThread` / `ThreeThread`（逻辑 + 渲染 + RHI）三种模式可切换，配合三缓冲 `FrameSynchronizer`

### GPU 特性

- **[x] Bindless 渲染**
  基于 descriptor-indexing 的全局描述符集（set 2）：4096 个 texture2D + 256 个 cube + 64 个 sampler 槽位，支持槽位分配/释放/更新

- **[x] GPU 神经网络（NNE）**
  通过 31 个 compute shader 实现 GPU 全量训练：Linear / Conv2D / MultiHeadAttention / BatchNorm / LayerNorm / Dropout / Residual 模块；ReLU / GELU / SiLU 等激活函数；SoftmaxCrossEntropy 损失函数；SGD / Adam / AdamW 优化器；学习率调度器；模型保存/加载

- **[x] GPU 内存子分配**
  UE 风格：`VK_DeviceMemoryManager`（页级）→ `VK_MemoryResourceFragmentAllocator`（子分配）→ `VK_ResourcePool`（按描述符哈希的瞬态复用）

- **[x] Dynamic Rendering**
  Vulkan 1.3 `vkCmdBeginRendering` 路径，自动回退到传统 RenderPass

- **[x] 管线缓存持久化**
  `PipelineCache.bin` 启动时加载/关闭时保存；PSO 按内容哈希缓存

- **[x] Shader 反射绑定**
  SPIRV-Reflect 驱动描述符集布局和按名称资源绑定（`srb->SetResource("name", ...)`）

- **[x] Staging Buffer 管理器**
  池化的 staging buffer，基于 fence 的延迟回收，用于纹理/缓冲区上传

- **[x] 曲面细分**
  `PatchList` 拓扑 + hull/domain shader 阶段；`.tesc/.tese` shader 文件

- **[x] 间接绘制/分发**
  `DrawIndirect` / `DrawIndexedIndirect` / `DispatchIndirect`，支持 `Storage|Indirect` 缓冲区（GPU 写入 draw args）

- **[x] Storage Images（UAV 纹理）**
  `ENUM_TYPE_STORAGE` 纹理 + `TransitionTextureState(UnorderedAccess)` / `ResourceBarrier`；GLSL 中 `image2D`/`uimage2D`；已验证 `imageAtomicMin/Add`（R32U 格式）

- **[ ] 虚拟纹理**
  四叉树页布局原型（`src/Runtime/Render/Core/VirtualTexture/`），WIP

- **[ ] GPU-Driven 渲染**
  Culling/depth-reduce shader 已就绪，C++ 侧暂存于 `backup/`，WIP

### UI 框架

- **[x] 多后端 UI 框架**
  `UISystem` 抽象基类（虚方法，非纯虚，后端按需 override）→ `RmlUISystem`（RmlUi 后端）；`UIManager` 单例门面（持有 `UISystem*`，多态分发）；`UIDataModelBinder` 抽象数据绑定器（零 `void*`，零 RmlUi 类型穿透）；`UIRenderer` 瘦身抽象渲染器（仅 3 个虚方法）；`UIInputBridge` 抽象输入路由；RTTR 驱动的声明式 `UIWidget` 框架 + `UI_BIND` 注解 + MetaParser 代码生成 + `UIWidgetManager` 生命周期管理

---

## 编辑器

`Editor` 目标是一个基于 ImGui（docking + 多视口）的可视化 RenderGraph 编辑器：

- **RenderGraphPanel** — 节点编辑器画布：添加/删除 Pass 和资源节点、连接 Pin、右键菜单（由内置 `PassRegistry` 驱动，含 GBuffer、DepthPre、Shadow、Lighting、SSAO、Bloom、TAA、ToneMapping 等预设）
- **PropertiesPanel / OutlinePanel** — 通过 `EventBus` 实现选区同步的属性检查和概览
- **Command 系统** — 支持事务和命令合并的撤销/重做历史
- **TemplateLibrary** — 内置 GBuffer / Lighting / Shadow / PostProcess 模板，支持用户 `.rgtemplate.json`
- **GraphValidator + ConnectionValidator** — 编译前检查（环、连通性、命名、Pin 类型规则）
- **AutoSaveService** — 定时原子保存，支持崩溃恢复
- **序列化** — 图以可读 `.rgraph.json` 格式保存/加载（v2 模式）；每个图形 Sample 导出一份可供编辑器打开

---

## Sample（示例）

> 主要用验证引擎各功能模块

| Target | 演示内容 |
|---|---|
| `RendererSample-HelloTriangle` | 最小 RenderGraph Pass：绘制三角形；导出 `hello_triangle.rgraph.json` |
| `RendererSample-Texture` | 异步纹理资产加载（DDS）+ 采样全屏四边形；导出 `texture.rgraph.json` |
| `RendererSample-CubeMap` | DDS 立方体贴图天空盒渲染；导出 `cubemap.rgraph.json` |
| `RendererSample-Reflection` | MetaParser + rttr 反射控制台演示（属性 get/set、方法调用） |
| `RendererSample-Bindless` | Bindless PBR：纹理分配到全局描述符堆，shader 中按索引采样 |
| `RendererSample-NeuralNetwork` | GPU 训练 MLP 对二维螺旋数据集分类（前向/反向/更新全在 compute 中完成） |
| `RendererSample-NNE_MNIST_CNN` | MNIST 风格 CNN：Conv2D + BatchNorm + Dropout + AdamW，模型保存/加载（合成数据） |
| `RendererSample-NNE_Transformer` | 微型 Transformer：MultiHeadAttention + LayerNorm + Residual + GELU + 余弦学习率 |
| `RendererSample-Fluid2D` | 交互式二维稳定流体模拟（鼠标注入力/染料，Jacobi 压力求解），水墨风格渲染，全部在 storage buffer 上完成 |
| `RendererSample-Fluid3D` | GPU FLIP 流体：MAC 网格压力投影、定点原子 P2G、GPU 自由链表粒子回收、屏幕空间水体渲染（含泡沫）；6 个 RDG Pass，保留 storage-image 纹理 + 手动布局转换 |
| `RendererSample-Ocean` | FFT 频谱海浪：compute IFFT 链，storage-buffer 导出到图形 PSO，顶点拉取网格 |
| `RendererSample-VolumetricCloud` | 3D 纹理参考：Hillaire 天空 LUT + Nubis 风格光线步进云层；单次 compute 烘焙可平铺 3D Perlin-Worley 噪声到 `image3D`，每帧 sky-view LUT + 半分辨率云层步进，`sampler3D` 读取 |
| `RendererSample-Mesh` | 顶点输入参考：MeshAsset obj 加载 → `Vertex\|Dynamic` VB/IB，`vertex_input_layout` PSO，OrbitCameraController + SceneView，DrawIndexed vs DrawIndexedIndirect 切换 |
| `RendererSample-RmlUI` | RmlUI 游戏 UI 演示：血量条、分数、计时器 + `UI_BIND` 注解；多后端 `UISystem` 架构；`UIDataModelBinder` 抽象绑定（零 `void*`，Sample 层零 Rml 类型引用）|
| `RendererSample-VirtualTexture` | 虚拟纹理四叉树页布局原型 |

运行任意 Sample（shader 通过相对路径 `Shader/...` 加载）：

```batch
xmake build RendererSample-Fluid2D
xmake run RendererSample-Fluid2D
```

---

## 项目结构

```
src/
  Runtime/              # 引擎核心库
    Core/               # 基础类型与宏（ConstDefine.h）、反射运行时
    RHI/                # 渲染硬件抽象层（平台无关）
      Vulkan/           #   Vulkan 后端（VK_* 类）
    Render/Core/        # RenderGraph 系统（+ 序列化、验证、Pass 注册）
    Application/        # 窗口 / 主循环（GLFW）
    Asset/              # Mesh / 纹理 / 材质资产
    Platform/           # 平台抽象层
    UI/                 # 多后端 UI 框架（UISystem、UIManager、RmlUI 后端）
      Widget/           #   RTTR 驱动的声明式 Widget 框架（UIWidgetManager）
      RmlUI/            #   RmlUI 具体后端（RmlUISystem、RmlUIRenderer、RmlDataModelBinder）
    GenCode/            # 自动生成代码（FlatBuffers schema、嵌入式 SPIR-V）
  Editor/               # 编辑器应用
    EditorRender/       # EditorRenderPipeline + EditorUI（ImGui）
    UI/                 # Node/Pin/Link/Panel 框架 + RenderGraphEditor
  Sample/               # RendererSample-* 源码
  Reflect/meta_parser/  # MetaParser（基于 libclang 的代码生成工具）
  _Generated/           # 反射 & 序列化生成代码（勿手动修改）
resource/               # Shader（GLSL + SPIR-V）、纹理（含 Sponza）、编辑器资产
template/               # Mustache 模板（用于反射代码生成）
```

---

## 第三方依赖

通过 xmake 包管理获取：`vulkansdk`、`glfw 3.4`、`glm`、`imgui 1.89.9-docking`、`assimp`、`tinyobjloader`、`gli`、`lz4`、`nlohmann_json`、`rttr`、`boost 1.84`、`flatbuffers 1.12`、`glslang`。

源码内嵌：`RmlUi`（HTML/CSS UI 中间件）、`TaskScheduler`（多线程任务调度器）、`SPIRV-Reflect`、`stb_image`、`VulkanMemoryAllocator`、`imgui-node-editor`（ax::NodeEditor）、`libclang`（MetaParser 用）。
