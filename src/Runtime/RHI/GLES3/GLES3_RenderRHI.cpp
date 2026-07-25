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

	// Read actual drawing buffer size from GL — the canvas may have been
	// created at native device resolution (e.g. 852x393 from tt.createCanvas()
	// vs the hardcoded 1280x960). Use the real size for the viewport.
	int actual_w = 0, actual_h = 0;
	emscripten_webgl_get_drawing_buffer_size(m_gl_context, &actual_w, &actual_h);
	if (actual_w > 0 && actual_h > 0)
	{
		std::cout << "[GLES3] Drawing buffer: " << actual_w << "x" << actual_h
		          << " (requested: " << width << "x" << height << ")" << std::endl;
		width  = actual_w;
		height = actual_h;
	}

	m_viewport = new GLES3_Viewport(m_gl_context, (UInt32)width, (UInt32)height);
	return m_viewport;
}

Shader* GLES3_RenderRHI::CreateShader(CONST ShaderDesc& desc, CONST ShaderDataPayload& data)
{
	auto* shader = new GLES3_Shader(desc, data);

	// Auto-store GLSL source from wgsl_source field (reused for GLSL ES source
	// in the GLES3 backend). Sample code no longer needs to manually cast to
	// GLES3_Shader and call SetGLSLSource — the backend handles it transparently.
	if (!data.wgsl_source.empty())
	{
		shader->SetGLSLSource(data.wgsl_source);
	}
	return shader;
}

Buffer* GLES3_RenderRHI::CreateBuffer(CONST BufferDesc& buffer_desc)
{
	auto* buf = new GLES3_Buffer(buffer_desc);

	GLuint gl_buf = 0;
	glGenBuffers(1, &gl_buf);
	CHECK_WITH_LOG(gl_buf == 0, "GLES3: glGenBuffers failed");

	GLenum target = buf->GetGLTarget();
	GLenum usage = TranslateBufferType_ToUsage(buffer_desc.type);

	glBindBuffer(target, gl_buf);
	glBufferData(target, (GLsizeiptr)buffer_desc.size, nullptr, usage);
	glBindBuffer(target, 0);

	buf->SetGLBuffer(gl_buf);
	return buf;
}

Texture* GLES3_RenderRHI::CreateTexture(CONST TextureDesc& texture_desc)
{
	auto* tex = new GLES3_Texture(texture_desc);

	// Phase 0: default framebuffer doesn't need a GL texture
	if (texture_desc.usage == ENUM_TEXTURE_USAGE_TYPE::ENUM_TYPE_COLOR_ATTACHMENT &&
	    texture_desc.width == 1280) // heuristic: swapchain backbuffer
	{
		return tex;
	}

	// Create GL texture
	GLuint gl_tex = 0;
	glGenTextures(1, &gl_tex);
	CHECK_WITH_LOG(gl_tex == 0, "GLES3: glGenTextures failed");

	GLenum internal_fmt = TranslateTextureFormat_Internal(texture_desc.format);
	GLenum pixel_fmt = TranslateTextureFormat_Pixel(texture_desc.format);
	GLenum pixel_type = TranslateTextureFormat_Type(texture_desc.format);

	GLenum tex_target = GL_TEXTURE_2D;
	if (texture_desc.type == ENUM_TEXTURE_TYPE::ENUM_TYPE_3D)
		tex_target = GL_TEXTURE_3D;
	else if (texture_desc.type == ENUM_TEXTURE_TYPE::ENUM_TYPE_CUBE_MAP)
		tex_target = GL_TEXTURE_CUBE_MAP;

	glBindTexture(tex_target, gl_tex);

	if (tex_target == GL_TEXTURE_2D)
	{
		glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internal_fmt,
			(GLsizei)texture_desc.width, (GLsizei)texture_desc.height,
			0, pixel_fmt, pixel_type, nullptr);
	}

	// Default sampling params
	glTexParameteri(tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(tex_target, 0);

	tex->SetGLTexture(gl_tex);
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

	// GLES 3.0: VAO creation is deferred to draw time (glVertexAttribPointer
	// requires a bound VBO). The vertex layout is stored in the PSO and
	// applied in GLES3_CommandBuffer::Draw/DrawIndexed.
	GLuint vao = 0;
	if (!desc.vertex_input_layout.empty())
	{
		glGenVertexArrays(1, &vao);
		// VAO is created but attributes are set at draw time
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
