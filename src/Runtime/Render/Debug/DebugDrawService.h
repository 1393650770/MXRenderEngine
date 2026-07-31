#pragma once
#ifndef _DEBUG_DRAW_SERVICE_
#define _DEBUG_DRAW_SERVICE_

#include "Core/ConstDefine.h"
#include <glm/glm.hpp>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)
class CommandList;
class Buffer;
class RenderPipelineState;
class ShaderResourceBinding;
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Render)

// Singleton debug line-drawing service (world-space, batched into one
// vertex buffer per frame). Sample code calls Draw* between BeginFrame and
// EndFrame, then Render(cmd, view_proj) inside a pass execute lambda.
MYRENDERER_BEGIN_CLASS(DebugDrawService)
#pragma region METHOD
public:
	static DebugDrawService& METHOD(Get)();
	static void METHOD(Create)();
	static void METHOD(Destroy)();

	void METHOD(BeginFrame)();
	void METHOD(DrawLine)(glm::vec2 a, glm::vec2 b, glm::vec3 color);
	void METHOD(DrawRect)(glm::vec2 min, glm::vec2 max, glm::vec3 color);
	void METHOD(DrawCircle)(glm::vec2 center, Float32 radius, glm::vec3 color, UInt32 segments = 24);
	void METHOD(DrawAABB)(glm::vec2 min, glm::vec2 max, glm::vec3 color);
	void METHOD(EndFrame)();
	void METHOD(Render)(RHI::CommandList* cmd, CONST glm::mat4& view_proj);

protected:

private:
	DebugDrawService() MYDEFAULT;
	~DebugDrawService();

#pragma endregion

#pragma region MEMBER
public:

protected:
	static DebugDrawService* s_instance;

	struct LineVertex
	{
		glm::vec2 pos;
		glm::vec3 color;
	};
	Vector<LineVertex> line_buffer_;
	RHI::Buffer* vertex_buffer_ = nullptr;    // Vertex|Dynamic
	RHI::Buffer* mvp_buffer_ = nullptr;       // Storage|Dynamic (mat4)
	RHI::RenderPipelineState* pso_ = nullptr;
	RHI::ShaderResourceBinding* srb_ = nullptr;
	Bool initialized_ = false;

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // Render
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _DEBUG_DRAW_SERVICE_