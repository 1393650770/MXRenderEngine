#if PLATFORM_GLES3

#include "RHI/GLES3/GLES3_RenderRHI.h"
#include "RHI/GLES3/GLES3_CommandBuffer.h"
#include "RHI/GLES3/GLES3_Viewport.h"
#include "RHI/GLES3/GLES3_Texture.h"
#include "RHI/GLES3/GLES3_Shader.h"
#include "RHI/GLES3/GLES3_PipelineState.h"
#include "RHI/GLES3/GLES3_Buffer.h"
#include "RHI/GLES3/GLES3_Utils.h"
#include "RHI/RenderEnum.h"
#include <iostream>
#include <cstring>
#include <emscripten/html5.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
MYRENDERER_BEGIN_NAMESPACE(GLES3)

// Helper: compile a single GLSL ES shader stage
static GLuint CompileGLShader(GLenum stage, const char* source)
{
	GLuint shader = glCreateShader(stage);
	CHECK_WITH_LOG(shader == 0, "GLES3: glCreateShader failed");

	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
	{
		GLint info_len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
		if (info_len > 0)
		{
			Vector<Char> log(info_len + 1);
			glGetShaderInfoLog(shader, info_len, nullptr, log.data());
			std::cout << "[GLES3] Shader compile error: " << log.data() << std::endl;
		}
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

// Helper: compile and link a GLES3 program from vertex + fragment shaders
static GLuint CreateGLProgram(const char* vs_source, const char* fs_source)
{
	GLuint vs = CompileGLShader(GL_VERTEX_SHADER, vs_source);
	CHECK_WITH_LOG(vs == 0, "GLES3: vertex shader compilation failed");

	GLuint fs = CompileGLShader(GL_FRAGMENT_SHADER, fs_source);
	CHECK_WITH_LOG(fs == 0, "GLES3: fragment shader compilation failed");

	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);

	GLint linked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked)
	{
		GLint info_len = 0;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_len);
		if (info_len > 0)
		{
			Vector<Char> log(info_len + 1);
			glGetProgramInfoLog(program, info_len, nullptr, log.data());
			std::cout << "[GLES3] Program link error: " << log.data() << std::endl;
		}
		glDeleteProgram(program);
		program = 0;
	}

	// Detach and delete shaders after linking (they're kept alive by the program)
	glDetachShader(program, vs);
	glDetachShader(program, fs);
	glDeleteShader(vs);
	glDeleteShader(fs);

	return program;
}

#pragma region INIT_METHOD

void GLES3_RenderRHI::Init(RenderFactory* render_factory)
{
	// GLES3 on Emscripten: the GL context is created in the platform window
	// (EmscriptenGLWindow) via emscripten_webgl_create_context, which must
	// happen on the browser main thread before anything else.
	// This Init() is called from PlatformCreateDynamicRHI() after the factory
	// object is configured. The actual context creation happens in
	// EmscriptenGLWindow::InitRHIAndViewport().
	//
	// For now, we store the factory config (mainly threading_mode=Single)
	// and allow CreateViewport to be called later with the canvas handle.
	std::cout << "[GLES3] RenderRHI::Init (context will be created on first CreateViewport)" << std::endl;
	(void)render_factory;
}

void GLES3_RenderRHI::PostInit()
{
	// No-op for Single-threaded GLES3 backend
}

void GLES3_RenderRHI::Shutdown()
{
	// Release PSO storage (GL programs are owned by the RHI)
	for (auto* pso : m_pso_storage)
	{
		if (pso) delete pso;
	}
	m_pso_storage.clear();

	if (m_immediate_cmd) { delete m_immediate_cmd; m_immediate_cmd = nullptr; }

	if (m_gl_context)
	{
		emscripten_webgl_destroy_context(m_gl_context);
		m_gl_context = 0;
	}

	std::cout << "[GLES3] Shutdown complete." << std::endl;
}

#pragma endregion

#pragma region CREATE_RESOURCE

Viewport* GLES3_RenderRHI::CreateViewport(void* window_handle, Int width, Int height, Bool is_full_screen)
{
	const char* canvas_selector = "#canvas";
	if (window_handle) canvas_selector = static_cast<const char*>(window_handle);

	// Create WebGL context if not already created
	if (!m_gl_context)
	{
		EmscriptenWebGLContextAttributes attrs;
		emscripten_webgl_init_context_attributes(&attrs);

		attrs.majorVersion = 2;  // WebGL 2.0 = GLES 3.0
		attrs.minorVersion = 0;
		attrs.alpha = true;
		attrs.depth = true;
		attrs.stencil = true;
		attrs.antialias = false;
		attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;
		attrs.explicitSwapControl = false;  // browser auto-swap at rAF end
		attrs.renderViaOffscreenBackBuffer = false;

		// Determine canvas target
		String target = canvas_selector;
		if (!target.empty() && target[0] == '#')
			target = target.substr(1);  // remove '#' for emscripten_webgl_create_context

		m_gl_context = emscripten_webgl_create_context(target.c_str(), &attrs);
		CHECK_WITH_LOG(m_gl_context == 0, "GLES3: emscripten_webgl_create_context failed");
		CHECK_WITH_LOG(m_gl_context < 0, "GLES3: emscripten_webgl_create_context returned error");

		// Make context current
		EMSCRIPTEN_RESULT res = emscripten_webgl_make_context_current(m_gl_context);
		CHECK_WITH_LOG(res != EMSCRIPTEN_RESULT_SUCCESS, "GLES3: emscripten_webgl_make_context_current failed");

		std::cout << "[GLES3] WebGL 2.0 context created: " << width << "x" << height
		          << ", GL version: " << glGetString(GL_VERSION)
		          << ", GLSL: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;

		// Enable scissor test (required for glClear to respect viewport)
		glEnable(GL_SCISSOR_TEST);
	}
	else
	{
		emscripten_webgl_make_context_current(m_gl_context);
	}

	m_viewport = new GLES3_Viewport(m_gl_context, (UInt32)width, (UInt32)height);
	return m_viewport;
}

Shader* GLES3_RenderRHI::CreateShader(CONST ShaderDesc& desc, CONST ShaderDataPayload& data)
{
	// Phase 0: GLSL ES source is embedded in the shader data payload.
	// The source is stored as a string in data.data (reinterpreted from uint32).
	// Phase 1+: SPIRV-Cross converts SPIR-V to GLSL ES source before this point.

	auto* shader = new GLES3_Shader(desc, data);

	// If wgsl_source is populated (reusing the field for GLSL source until a
	// dedicated glsl_source field is added), use it directly.
	// Otherwise extract from spirv_data (Phase 1 SPIRV-Cross path).
	return shader;
}

Buffer* GLES3_RenderRHI::CreateBuffer(CONST BufferDesc& buffer_desc)
{
	// Minimal: HelloTriangle doesn't use buffers (hardcoded triangle in shader).
	auto* buf = new GLES3_Buffer(buffer_desc);
	return buf;
}

Texture* GLES3_RenderRHI::CreateTexture(CONST TextureDesc& texture_desc)
{
	// Minimal: HelloTriangle uses default framebuffer only.
	auto* tex = new GLES3_Texture(texture_desc);
	return tex;
}

RenderPipelineState* GLES3_RenderRHI::CreateRenderPipelineState(CONST RenderGraphiPipelineStateDesc& desc)
{
	// Get vertex shader GLSL source
	CONST auto* vs_shader = static_cast<CONST GLES3_Shader*>(desc.shaders[ENUM_SHADER_STAGE::Shader_Vertex]);
	CHECK_WITH_LOG(vs_shader == nullptr, "GLES3: PSO missing vertex shader");

	// Get fragment shader GLSL source
	CONST auto* fs_shader = static_cast<CONST GLES3_Shader*>(desc.shaders[ENUM_SHADER_STAGE::Shader_Pixel]);
	CHECK_WITH_LOG(fs_shader == nullptr, "GLES3: PSO missing fragment shader");

	// Compile and link GL program
	GLuint program = CreateGLProgram(
		vs_shader->GetGLSLSource().c_str(),
		fs_shader->GetGLSLSource().c_str());
	CHECK_WITH_LOG(program == 0, "GLES3: CreateGLProgram failed");

	auto* pso = new GLES3_PipelineState(desc);
	pso->SetGLProgram(program);

	// Create VAO if there's a vertex input layout (Phase 1+).
	// Phase 0 (HelloTriangle): gl_VertexIndex in shader, no VAO needed.
	GLuint vao = 0;
	if (!desc.vertex_input_layout.bindings.empty())
	{
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		// Phase 2+: set up vertex attributes from vertex_input_layout
		glBindVertexArray(0);
	}
	pso->SetVAO(vao);

	// Take ownership: store in m_pso_storage, external code must not delete
	m_pso_storage.push_back(pso);
	return pso;
}

ComputePipelineState* GLES3_RenderRHI::CreateComputePipelineState(CONST ComputePipelineStateDesc& desc)
{
	// GLES 3.0 doesn't support compute shaders (requires ES 3.1+)
	(void)desc;
	return nullptr;
}

RenderPass* GLES3_RenderRHI::CreateRenderPass(CONST RenderPassDesc& desc)
{
	// GLES3 has no persistent RenderPass object (equivalent is inline glBindFramebuffer)
	(void)desc;
	return nullptr;
}

FrameBuffer* GLES3_RenderRHI::CreateFrameBuffer(CONST FrameBufferDesc& desc)
{
	// Phase 0: GLES3 uses the default framebuffer (0) or inline FBO.
	// Phase 1+: could create a GL FBO here for RenderGraph transient resources.
	(void)desc;
	return nullptr;
}

void* GLES3_RenderRHI::MapBuffer(Buffer* buffer, ENUM_MAP_TYPE map_type, ENUM_MAP_FLAG map_flag)
{
	return buffer->Map(map_type, map_flag);
}

void GLES3_RenderRHI::UnmapBuffer(Buffer* buffer)
{
	buffer->Unmap();
}

#pragma endregion

#pragma region DRAW

CommandList* GLES3_RenderRHI::GetImmediateCommandList()
{
	if (!m_immediate_cmd)
	{
		m_immediate_cmd = new GLES3_CommandBuffer();
	}
	m_immediate_cmd->Begin();
	return m_immediate_cmd;
}

CommandList* GLES3_RenderRHI::GetCommandListForQueue(ENUM_QUEUE_TYPE queue_type)
{
	return GetImmediateCommandList();
}

void GLES3_RenderRHI::SubmitCommandList(CommandList* command_list)
{
	// Single-threaded: commands already executed. Just End() to flush.
	if (command_list) command_list->End();
}

void GLES3_RenderRHI::SubmitCommandListForQueue(CommandList* cmd_list, ENUM_QUEUE_TYPE queue_type)
{
	SubmitCommandList(cmd_list);
}

void GLES3_RenderRHI::RenderEnd()
{
	// No-op for Single-threaded mode
}

CommandList* GLES3_RenderRHI::GetWriteCommandList()
{
	return GetImmediateCommandList();
}

CommandList* GLES3_RenderRHI::GetRHICmdListForPresent()
{
	return GetImmediateCommandList();
}

void GLES3_RenderRHI::SwapCommandLists()
{
	// No-op for Single-threaded mode
}

Bool GLES3_RenderRHI::IsReplayDone() CONST
{
	return true;  // Single-threaded: replay is a no-op
}

void GLES3_RenderRHI::StartRHIThread()
{
	// No-op: GLES3 backend is Single-threaded only
}

void GLES3_RenderRHI::StopRHIThread()
{
	// No-op: GLES3 backend is Single-threaded only
}

BindlessManager* GLES3_RenderRHI::GetBindlessManager()
{
	return nullptr;  // GLES3 doesn't support bindless descriptors
}

#pragma endregion

MYRENDERER_END_NAMESPACE  // GLES3
MYRENDERER_END_NAMESPACE  // RHI
MYRENDERER_END_NAMESPACE  // MXRender

#endif // PLATFORM_GLES3
