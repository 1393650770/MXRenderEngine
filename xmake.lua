if is_plat("windows") then
    add_requireconfs("**.glfw", {override = true, version = "3.4",configs = {shared = true,debug=true}})
end
if is_plat("android") then
    add_requires("glm","tinyobjloader","nlohmann_json","gli","optick","rttr")
    add_syslinks("vulkan", "android") -- libvulkan.so + libandroid.so on device
elseif is_plat("wasm") then
    -- GLES3 Phase 0: header-only libs only (flatbuffers/rttr require cmake emscripten fixes)
    add_requires("glm","nlohmann_json")
else
    add_requires("vulkansdk", "glm","tinyobjloader","nlohmann_json","gli","optick","rttr")
end
if not is_plat("android","wasm") then
    add_requires("assimp","lz4")
end
if is_plat("android") then
    add_requires("imgui v1.89.9-docking", {configs = { debug = true, shared = true }})
elseif is_plat("wasm") then
    add_requires("imgui v1.89.9-docking", {configs = { debug = true }})
else
    add_requires("imgui v1.89.9-docking", {configs = { glfw_vulkan = true, debug = true, shared = true }})
end
if not is_plat("wasm") then
    add_requires("flatbuffers v1.12.0", { system = false, configs = { vs_runtime = "MT" } })
end
if not is_plat("android","wasm") then
    add_requires("boost",{ version = "1.84.0",configs = {shared = true,debug=true,cmake=false}})
end
if is_plat("windows") then
    add_requires("glfw 3.4", {configs = {shared = true,debug=true}})
end
if not is_plat("wasm") then
    add_requires("glslang", {configs = {binaryonly = true}})
end
if not is_plat("android","wasm") then
	add_requires("freetype")
end
add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

rule("module")
    on_load(function (target)
        target:set("kind", "static")  -- always static: avoids DLL CRT conflicts with flatbuffers(MT)
    end)
rule_end()
includes("src/**/xmake.lua")

rule("flatbufferFile")
    set_extensions(".fbs")
    --on_build_file(function (target, sourcefile, opt)
    --    print("Compiler Fbs")
    --    os.cp(sourcefile, path.join(target:targetdir(), path.basename(sourcefile)))
    --end)


function PlatformSettings()
    if is_plat("windows") then
        add_defines("PLATFORM_WIN32")
        add_syslinks("comdlg32")
    elseif is_plat("linux") then
        add_defines("PLATFORM_LINUX")
    elseif is_plat("macosx") then
        add_defines("PLATFORM_MACOS")
    elseif is_plat("android") then
        add_defines("PLATFORM_ANDROID", "_XOPEN_SOURCE=700")
        add_cxflags("-Wno-c++11-narrowing")
        add_ldflags("-Wl,--unresolved-symbols=ignore-in-shared-libs", {force = true})
    elseif is_plat("wasm") then
        add_defines("PLATFORM_GLES3", "PLATFORM_EMSCRIPTEN")
        add_cxflags("-Wno-c++11-narrowing")
        if is_mode("release") then
            add_ldflags("-sFULL_ES3=1", "-sMAX_WEBGL_VERSION=2", "-sALLOW_MEMORY_GROWTH=1",
                "-Oz", "-flto", "-sASSERTIONS=0", {force = true})
        else
            add_ldflags("-sFULL_ES3=1", "-sMAX_WEBGL_VERSION=2", "-sALLOW_MEMORY_GROWTH=1", {force = true})
        end
    end
end

function CommonLibrarySetting()
    set_languages("clatest", "cxx20")
    if is_plat("windows") then set_runtimes("MT") end  -- static CRT matches flatbuffers
    PlatformSettings()
    add_headerfiles("src/Runtime/**.h")
    add_files("src/Runtime/**.cpp")
    -- Exclude platform-specific files
    if is_plat("android") then
        remove_files("src/Runtime/Platform/Desktop/**.cpp")
        remove_files("src/Runtime/Application/Android/TouchOrbitCamera.cpp")
        remove_files("src/Runtime/Tool/MeshLoader.cpp")
        remove_files("src/Runtime/Asset/MeshAsset.cpp")
    elseif is_plat("wasm") then
        remove_files("src/Runtime/RHI/Vulkan/**")
        remove_files("src/Runtime/RHI/WGPU/**")
        remove_files("src/Runtime/Platform/Win/**")
        remove_files("src/Runtime/Platform/Android/**")
        remove_files("src/Runtime/Platform/WGPU/**.cpp")  -- legacy path
        remove_files("src/Runtime/Platform/Desktop/**.cpp")  -- GLFW not on wasm
        remove_files("src/Runtime/Platform/Emscripten/EmscriptenWGPUWindow.cpp")  -- WebGPU backend (keep GLES3)
        remove_files("src/Runtime/Tool/MeshLoader.cpp")
        remove_files("src/Runtime/Asset/MeshAsset.cpp")
        remove_files("src/Runtime/Core/ReflectionRegister.cpp")  -- depends on rttr
        remove_files("src/Runtime/RHI/DeviceProfile.cpp")  -- depends on vulkan
        remove_files("src/Runtime/Asset/FileSystem/Win/WinFileSystem.cpp")  -- Win32 only
        remove_files("src/Runtime/Render/Core/RenderGraphSerializer.cpp")  -- depends on gli (Editor path)
        remove_files("src/Runtime/Render/Core/RenderGraphValidator.cpp")  -- Editor path
        remove_files("src/Runtime/Render/Core/PassRegistry.cpp")  -- depends on Editor/serializer
        remove_files("src/Runtime/Render/Core/VirtualTexture/**.cpp")  -- depends on gli
        remove_files("src/Runtime/Tool/BufferUtils.cpp")  -- depends on vk
        remove_files("src/Runtime/Tool/ToolUtils.cpp")  -- depends on gli
        remove_files("src/Runtime/Tool/TextureLoader.cpp")  -- depends on gli
        remove_files("src/Runtime/Asset/TextureAsset.cpp")  -- depends on gli/TextureLoader
        remove_files("src/ThirdParty/spv_reflect/**")  -- SPIR-V only, not needed for GLES3
        remove_files("src/Runtime/UI/Widget/**")  -- depends on rttr
        remove_files("src/Runtime/UI/UIBase.cpp")  -- depends on Widget/rttr
        remove_files("src/Runtime/UI/UIManager.cpp")  -- depends on Widget/rttr
        remove_files("src/Runtime/UI/UIRenderPass.cpp")  -- not needed for GLES3 HelloTriangle
        remove_files("src/ThirdParty/RmlUi/Source/Core/FontEngineDefault/**")  -- needs freetype
        remove_files("src/ThirdParty/TaskScheduler/Scheduler/Include/Platform/Posix/**.cpp")  -- no pthread_attr_setaffinity_np on wasm
        remove_files("src/ThirdParty/TaskScheduler/Scheduler/Source/**.cpp")  -- TaskScheduler unsupported on wasm
    else
        remove_files("src/Runtime/Platform/Android/**.cpp")
        remove_files("src/Runtime/Platform/WGPU/**.cpp")
        remove_files("src/Runtime/RHI/WGPU/**")
    end
    -- Platform-specific file dialog: exclude the wrong platform implementation
    if is_plat("windows") then
        remove_files("src/Runtime/Platform/FileDialogStub.cpp")
    else
        remove_files("src/Runtime/Platform/Win/WinFileDialog.cpp")
    end
    add_headerfiles("src/ThirdParty/**.h")
    add_files("src/ThirdParty/stb_image/**.cpp")
    if not is_plat("wasm") then
        add_files("src/ThirdParty/spv_reflect/**.cpp")
    end
    add_files("src/ThirdParty/**.c")
    remove_files("src/ThirdParty/emsdk/**")     -- emsdk is a build tool, not library source
    remove_files("src/ThirdParty/tinygltf/examples/**")
    remove_files("src/ThirdParty/tinygltf/tests/**")
    remove_files("src/ThirdParty/tinygltf/models/**")
    remove_files("src/ThirdParty/tinygltf/tools/**")
    remove_files("src/ThirdParty/tinygltf/benchmark/**")
    remove_files("src/ThirdParty/tinygltf/experimental/**")
    remove_files("src/ThirdParty/tinygltf/wasm/**")
    remove_files("src/ThirdParty/TaskScheduler/Scheduler/Source/**.cpp")  -- _aligned_malloc removed in new MSVC
    -- RmlUI library (retained-mode game UI)
    add_includedirs("src/ThirdParty/RmlUi/Include", {public = true})
    add_files("src/ThirdParty/RmlUi/Source/Core/**.cpp|**/FontEngineDefault/**")
    add_files("src/ThirdParty/RmlUi/Source/Core/Elements/**.cpp")
    add_files("src/ThirdParty/RmlUi/Source/Core/Layout/**.cpp")
    add_files("src/ThirdParty/RmlUi/Source/Debugger/**.cpp")
    if not is_plat("android","wasm") then
        add_files("src/ThirdParty/RmlUi/Source/Core/FontEngineDefault/**.cpp")
        add_defines("RMLUI_FONT_ENGINE_FREETYPE")
    end
    add_defines("RMLUI_STATIC_LIB")
    add_includedirs("src/_Generated", {public = true})
    add_includedirs("src/Runtime", {public = true})
    add_includedirs("src/ThirdParty", {public = true})
    add_includedirs("src/ThirdParty/TaskScheduler/Scheduler/Include", {public = true})
    add_files("src/ThirdParty/TaskScheduler/Scheduler/Source/**.cpp")
    if is_plat("windows") then
        add_files("src/ThirdParty/TaskScheduler/Scheduler/Include/Platform/Windows/**.cpp")
    elseif is_plat("android") then
        add_files("src/ThirdParty/TaskScheduler/Scheduler/Include/Platform/Posix/**.cpp")
    elseif is_plat("wasm") then
        add_files("src/ThirdParty/TaskScheduler/Scheduler/Include/Platform/Posix/**.cpp")
    end
    if not is_plat("android","wasm") then
        add_packages("vulkansdk")
    end
    if is_plat("wasm") then
        -- Phase 0: header-only libs only (imgui/flatbuffers/rttr require cmake emscripten fixes)
        add_packages("glm","nlohmann_json")
    else
        add_packages("glm","tinyobjloader","imgui","nlohmann_json","gli","optick","flatbuffers","rttr")
    end
    if not is_plat("android","wasm") then
        add_packages("assimp","lz4","boost","freetype")
    end
    if is_plat("windows") then
        add_packages("glfw")
    end
end



function CompileFunc()
    print("[compile c++ reflection code] schema gen code..")
    local folders = {
        "$(projectdir)/src/Runtime/Core",
        "$(projectdir)/src/Runtime/Platform",
        "$(projectdir)/src/Runtime/Asset",
        "$(projectdir)/src/Runtime/Tool",
        "$(projectdir)/src/Runtime/UI",
        "$(projectdir)/src/Runtime/Application",
        "$(projectdir)/src/Sample",
        "$(projectdir)/src/Editor"
    }
    
    local headers = {}
    for _, folder_path in ipairs(folders) do
        local files_in_folder = os.files(path.join(folder_path, "**.h"), {recursive = true})
        for _, header in ipairs(files_in_folder) do
            table.insert(headers, header)
        end
    end

    local file = io.open("$(projectdir)/src/Reflect/precompile.info", "w")
    if file then
        for _, header in ipairs(headers) do
            file:write(header, ";")
        end
        file:close()
    end
    os.exec("$(projectdir)/src/Reflect/MetaParser.exe $(projectdir)/src/Reflect/precompile.info  $(projectdir)/src/Reflect/parser_header.h \"$(projectdir)/src;$(projectdir)/src/Runtime;$(projectdir)/src/ThirdParty;$(projectdir)/src/ThirdParty/RmlUi/Include\" * MXRender 0")
    print("----\n")
    
    print("[compile shader] shader to spirv..")
    if not is_plat("wasm") then
        local vulkan_sdk = find_package("vulkansdk")
        if vulkan_sdk then
            local glslang_validator_dir = vulkan_sdk["bindir"].."\\glslangValidator.exe"
            for _, shader_path in ipairs(os.files("$(projectdir)/resource/Shader/**|**.spv|**.bat|**.exe|**.h|**.glsl|**.wgsl")) do
                print("[compile shader] : "..shader_path)
                os.runv(glslang_validator_dir,{"-V", "-g", shader_path,"-o", shader_path..".spv"})
            end
        end
    else
        -- wasm target: browser WebGPU needs WGSL (SPIR-V not directly usable).
        -- WGSL files are hand-written (resource/Shader/**.wgsl) or converted via
        -- tint. GLSL->SPIR-V via host glslangValidator (VULKAN_SDK env), then
        -- tint SPIR-V->WGSL for shaders lacking a hand-written .wgsl.
        local vk_sdk = os.getenv("VULKAN_SDK")
        if vk_sdk and vk_sdk ~= "" then
            local glslang = path.join(vk_sdk, "Bin", "glslangValidator.exe")
            if os.isfile(glslang) then
                for _, shader_path in ipairs(os.files("$(projectdir)/resource/Shader/**|**.spv|**.bat|**.exe|**.h|**.glsl|**.wgsl")) do
                    print("[compile shader] : "..shader_path)
                    os.runv(glslang, {"-V", "-g", shader_path, "-o", shader_path..".spv"})
                end
            end
        end
        local tint_exe = os.getenv("TINT_PATH")
        if tint_exe and os.isfile(tint_exe) then
            for _, spv_path in ipairs(os.files("$(projectdir)/resource/Shader/**.spv")) do
                local wgsl_path = spv_path:gsub("%.spv$", ".wgsl")
                if not os.isfile(wgsl_path) then
                    print("[compile shader] tint: "..spv_path.." -> "..wgsl_path)
                    os.runv(tint_exe, {spv_path, "-o", wgsl_path})
                end
            end
        else
            print("[compile shader] wasm: tint not found (set TINT_PATH env); using hand-written .wgsl files")
        end
    end

    print("----\n")

    print("[compile flatbuffer] schema gen code..")
    local flatbuffer_dir = "$(projectdir)/src/Runtime/GenCode/Schema" --定位到flatbuffer flatc.exe的路径
    local flatbuffer_gen_dir = flatbuffer_dir.."/FlatbufferGen" 
    
    local flatc_dir =  flatbuffer_dir.."/flatc.exe"--flatbuffer_dir["bindir"].."\\flatc.exe" --获取到glslangValidator.exe的路径
    local flatbuffer_all_fbs_files =""
    print(flatc_dir)
    -- 遍历$(projectdir)/engine/shaders/中除了.spv后缀的所有文件
    for _, dir in ipairs(os.dirs(flatbuffer_dir.."/FlatbufferGen")) do
        print("Get Gen Dir..".. dir)
        flatbuffer_gen_dir = dir
        break
    end
    for _, flatbuffer_path in ipairs(os.files(flatbuffer_dir.."/**.fbs")) do
        flatbuffer_all_fbs_files = flatbuffer_all_fbs_files.." "..flatbuffer_path
        print("[compile flatbuffer] done: "..flatbuffer_path)
        os.runv(flatc_dir,{"-o" ,flatbuffer_gen_dir , "--cpp","--gen-object-api","--gen-compare","--gen-all","--schema","--natural-utf8","--defaults-json","--defaults-json","--gen-onefile","--reflect-types","--reflect-names", flatbuffer_path }) --执行系统命令
    end
    print("----\n")
end


function MoveResource(target)
    local root_taget_path = "$(buildir)"
    if is_plat("windows") then
        root_taget_path = root_taget_path .. "/windows"
    end
    if is_arch("x64") then
        root_taget_path = root_taget_path .. "/x64"
    end
    if is_mode("release") then
        root_taget_path = root_taget_path .. "/release"
    elseif is_mode("debug") then
        root_taget_path = root_taget_path .. "/debug"
    elseif is_mode("releasedbg") then
        root_taget_path = root_taget_path .. "/releasedbg"
    end
    local root_taget_shader_path = root_taget_path .. "/Shader"
    local root_taget_texture_path = root_taget_path .. "/Texture"
    local root_taget_editor_path = root_taget_path .. "/Editor"
    local root_taget_dataset_path = root_taget_path .. "/Dataset"
    local root_taget_lib_path = root_taget_path
    os.mkdir(root_taget_shader_path)
    os.mkdir(root_taget_dataset_path)
    for _, gen_shader_pth in ipairs(os.files("$(projectdir)/resource/Shader/**.spv")) do
        print("[copy shader] done :"..gen_shader_pth)
        os.cp(gen_shader_pth, root_taget_shader_path)
    end
    -- WebGPU/wasm: copy .wgsl shader sources (hand-written or tint-converted)
    for _, wgsl_pth in ipairs(os.files("$(projectdir)/resource/Shader/**.wgsl")) do
        print("[copy wgsl] done :"..wgsl_pth)
        os.cp(wgsl_pth, root_taget_shader_path)
    end
    os.cp("$(projectdir)/resource/Texture", root_taget_texture_path)
    if os.isdir("$(projectdir)/resource/Mesh") then
        os.cp("$(projectdir)/resource/Mesh", root_taget_path .. "/Mesh")
    end
    os.cp("$(projectdir)/resource/Editor", root_taget_editor_path)
if os.isdir("$(projectdir)/resource/RmlUI") then
	os.cp("$(projectdir)/resource/RmlUI", root_taget_path .. "/RmlUI")
end
if os.isdir("$(projectdir)/resource/Font") then
	os.cp("$(projectdir)/resource/Font", root_taget_path .. "/Font")
end
    if os.isdir("$(projectdir)/resource/Dataset") then
        os.cp("$(projectdir)/resource/Dataset", root_taget_dataset_path)
    end
    os.cp("$(projectdir)/libs/*", root_taget_lib_path)
    
end

function CommonProjectSetting()
    if is_plat("android") then
        set_kind("shared")
    else
        set_kind("binary")
    end
    set_languages("clatest", "cxx20")
    if is_plat("windows") then
        set_runtimes("MT")  -- static CRT to match flatbuffers(MT)
    end
    add_defines("RMLUI_STATIC_LIB")
    PlatformSettings()
    add_deps("Runtime")
    if not is_plat("wasm") then
        add_files("src/_Generated/**.cpp", {public = true})
    end
    add_includedirs("src/_Generated", {public = true})
    add_includedirs("src/Runtime", {public = true})
    add_includedirs("src/ThirdParty", {public = true})
    if not is_plat("android","wasm") then
        add_packages("vulkansdk")
    end
    if is_plat("wasm") then
        -- Phase 0: header-only libs only
        add_packages("glm","nlohmann_json")
    else
        add_packages("glm","tinyobjloader","imgui","flatbuffers","rttr","nlohmann_json")
    end
    if not is_plat("android","wasm") then
        add_packages("assimp","lz4","boost","glfw")
    end
end

target("CompileResource")
    set_kind("binary")
    set_languages("clatest", "cxx20")
    PlatformSettings()
    add_files("src/Runtime/GenCode/Schema/*.fbs", {rule = "flatbufferFile"})
    add_extrafiles("src/Runtime/GenCode/Schema/*.fbs", {build = false})
    add_extrafiles("resource/Shader/**|**.spv|**.bat|**.exe|**.glsl", {build = false})
    add_files("src/CompileResource.cpp") 
    set_group("CompileResource")
    before_build(CompileFunc)

target("Runtime")
    if is_plat("android") then
        on_load(function (target)
            -- Read NDK path from .xmake/paths.ini (key=value format)
            local function read_ini_key(filepath, key)
                local f = io.open(filepath, "r")
                if not f then return nil end
                for line in f:lines() do
                    local k, v = line:match("^%s*" .. key .. "%s*=%s*(.+)")
                    if k then f:close(); return v end
                end
                f:close(); return nil
            end
            local ini = os.projectdir() .. "/.xmake/paths.ini"
            local ndk_path = read_ini_key(ini, "NDK")
            if ndk_path then
                local glue_dir = ndk_path:trim() .. "/sources/android/native_app_glue"
                target:add("includedirs", glue_dir, {public = true})
                target:add("files", glue_dir .. "/android_native_app_glue.c")
            else
                raise("Cannot read NDK path from .xmake/paths.ini -- set NDK = your/path there")
            end
        end)
    end
    add_rules("module")
    if not is_plat("wasm") then
        add_packages("glslang")
    end
    if not is_plat("wasm") then
        add_rules("utils.glsl2spv", {outputdir = "$(projectdir)/src/Runtime/GenCode/Shader",bin2c = true})
        add_files("resource/Shader/**|**.spv|**.bat|**.exe|**.h|**.glsl|**.wgsl", {build = false})
    end
    set_group("Runtime")
    CommonLibrarySetting()
    add_deps("CompileResource")


target("Renderer")
    CommonProjectSetting()
    add_files("src/RendererApp.cpp") 
    set_group("Sample")
    after_build(MoveResource)

target("Editor")
    CommonProjectSetting()
    add_defines("IMGUI_DEFINE_MATH_OPERATORS")
    add_defines("IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING")
    add_headerfiles("src/Editor/**.h")
    add_packages("nlohmann_json")
    add_files("src/Editor/**.cpp") 
    add_includedirs("src/Editor")
    set_group("Editor")
    after_build(MoveResource)


target("RendererSample-HelloTriangle")
    CommonProjectSetting()
    add_files("src/Sample/1-HelloTriangle/HelloTriangle.cpp")
    set_group("Sample")
    after_build(MoveResource)

target("RendererSample-Texture")
    CommonProjectSetting()
    add_files("src/Sample/2-Texture/Texture.cpp")
    set_group("Sample")
    after_build(MoveResource)

target("RendererSample-CubeMap")
    CommonProjectSetting()
    add_files("src/Sample/3-CubeMap/CubeMap.cpp")
    set_group("Sample")
    after_build(MoveResource)


target("RendererSample-VirtualTexture")
    CommonProjectSetting()
    add_files("src/Sample/N-VirtualTexture/VirtualTexture.cpp")
    set_group("Sample")
    after_build(MoveResource)

target("RendererSample-Reflection")
    CommonProjectSetting()
    add_headerfiles("src/Sample/4-Reflect/**.h")
    add_files("src/Sample/4-Reflect/**.cpp")
    add_files("src/Sample/4-Reflect/Reflect.cpp")
    set_group("Sample")
    after_build(MoveResource)

target("RendererSample-Bindless")
    CommonProjectSetting()
    add_files("src/Sample/5-Bindless/Bindless.cpp")
    set_group("Sample")
    after_build(MoveResource)

target("RendererSample-NeuralNetwork")
    CommonProjectSetting()
    add_files("src/Sample/6-NeuralNetwork/NeuralNetwork.cpp")
    add_files("src/Sample/6-NeuralNetwork/Tensor.cpp")
    add_files("src/Sample/6-NeuralNetwork/Layer.cpp")
    add_files("src/Sample/6-NeuralNetwork/Optimizer.cpp")
    add_files("src/Sample/6-NeuralNetwork/Model.cpp")
    --  
    add_files("src/Sample/6-NeuralNetwork/Activation.cpp")
    add_files("src/Sample/6-NeuralNetwork/Normalization.cpp")
    add_files("src/Sample/6-NeuralNetwork/MNISTDataLoader.cpp")
    add_files("src/Sample/6-NeuralNetwork/Trainer.cpp")
    add_files("src/Sample/6-NeuralNetwork/ResidualBlock.cpp")
    add_files("src/Sample/6-NeuralNetwork/ConvLayer.cpp")
    add_files("src/Sample/6-NeuralNetwork/AttentionLayer.cpp")
    set_group("Sample")
    after_build(MoveResource)

--   NNE MNIST CNN Demo
target("RendererSample-NNE_MNIST_CNN")
    CommonProjectSetting()
    add_files("src/Sample/6-NeuralNetwork/NNE_MNIST_CNN.cpp")
    add_files("src/Sample/6-NeuralNetwork/Tensor.cpp")
    add_files("src/Sample/6-NeuralNetwork/Layer.cpp")
    add_files("src/Sample/6-NeuralNetwork/Optimizer.cpp")
    add_files("src/Sample/6-NeuralNetwork/Model.cpp")
    add_files("src/Sample/6-NeuralNetwork/Activation.cpp")
    add_files("src/Sample/6-NeuralNetwork/Normalization.cpp")
    add_files("src/Sample/6-NeuralNetwork/ConvLayer.cpp")
    add_files("src/Sample/6-NeuralNetwork/ResidualBlock.cpp")
    add_files("src/Sample/6-NeuralNetwork/AttentionLayer.cpp")
    set_group("Sample")
    after_build(MoveResource)

--   NNE Transformer Demo
target("RendererSample-NNE_Transformer")
    CommonProjectSetting()
    add_files("src/Sample/6-NeuralNetwork/NNE_Transformer.cpp")
    add_files("src/Sample/6-NeuralNetwork/Tensor.cpp")
    add_files("src/Sample/6-NeuralNetwork/Layer.cpp")
    add_files("src/Sample/6-NeuralNetwork/Optimizer.cpp")
    add_files("src/Sample/6-NeuralNetwork/Model.cpp")
    add_files("src/Sample/6-NeuralNetwork/Activation.cpp")
    add_files("src/Sample/6-NeuralNetwork/Normalization.cpp")
    add_files("src/Sample/6-NeuralNetwork/ResidualBlock.cpp")
    add_files("src/Sample/6-NeuralNetwork/AttentionLayer.cpp")
    add_files("src/Sample/6-NeuralNetwork/ConvLayer.cpp")
    set_group("Sample")
    after_build(MoveResource)

--   2D Fluid Simulation Demo
target("RendererSample-Fluid2D")
    CommonProjectSetting()
    add_files("src/Sample/7-Fluid2D/Fluid2D.cpp")
    set_group("Sample")
    after_build(MoveResource)

--   3D Fluid Simulation Demo (PBF particles + screen-space ink-wash rendering)
target("RendererSample-Fluid3D")
    CommonProjectSetting()
    add_files("src/Sample/8-Fluid3D/Fluid3D.cpp")
    set_group("Sample")
    after_build(MoveResource)

--   FFT Spectral Ocean Demo (Tessendorf spectrum + GPU IFFT + optical water shading)
target("RendererSample-Ocean")
    CommonProjectSetting()
    add_files("src/Sample/9-Ocean/Ocean.cpp")
    set_group("Sample")
    after_build(MoveResource)

target("RendererSample-Mesh")
    CommonProjectSetting()
    add_files("src/Sample/11-Mesh/MeshSample.cpp")
    set_group("Sample")
    after_build(MoveResource)

--   Volumetric Cloud + Atmosphere Demo (Hillaire sky LUTs + Nubis-style raymarched clouds)
target("RendererSample-VolumetricCloud")
    CommonProjectSetting()
    add_files("src/Sample/10-VolumetricCloud/VolumetricCloud.cpp")
    set_group("Sample")
    after_build(MoveResource)

--   RmlUI Game UI Demo (retained-mode UI with data binding)
target("RendererSample-RmlUI")
    CommonProjectSetting()
    add_files("src/Sample/12-RmlUI/RmlUIDemo.cpp")
    set_group("Sample")
    -- Copy RmlUI resources unconditionally before every build (RML changes
    -- don't trigger C++ recompilation; before_build ensures latest copy).
    before_build(function (target)
        local outdir = target:targetdir()
        if os.isdir("$(projectdir)/resource/RmlUI") then
            os.cp("$(projectdir)/resource/RmlUI", outdir .. "/RmlUI")
        end
        if os.isdir("$(projectdir)/resource/Font") then
            os.cp("$(projectdir)/resource/Font", outdir .. "/Font")
        end
    end)
    after_build(MoveResource)

--   MiniGame GLES3 HelloTriangle (Phase 0: wasm-only GLES3 RHI backend validation)
target("MiniGame-HelloTriangle")
    if is_plat("wasm") then
        CommonProjectSetting()
        add_files("src/Sample/MiniGame-HelloTriangle/HelloTriangle.cpp")
    else
        set_kind("binary")
        add_deps("Runtime")
    end
    set_group("Sample")

target("MiniGame-Texture")
    if is_plat("wasm") then
        CommonProjectSetting()
        add_files("src/Sample/MiniGame-Texture/Texture.cpp")
    else
        set_kind("binary")
        add_deps("Runtime")
    end
    set_group("Sample")

target("MiniGame-Mesh")
    if is_plat("wasm") then
        CommonProjectSetting()
        add_files("src/Sample/MiniGame-Mesh/Mesh.cpp")
    else
        set_kind("binary")
        add_deps("Runtime")
    end
    set_group("Sample")



