#pragma once
#ifndef _GLTF_LOADER_
#define _GLTF_LOADER_

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE  // use our own stb_image from ThirdParty
#include "tinygltf/tiny_gltf.h"

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Asset)
MYRENDERER_BEGIN_NAMESPACE(glTF)

struct glTFPrimitive
{
	UInt32 index_offset = 0, index_count = 0;
	UInt32 vertex_offset = 0, vertex_count = 0;
	Int material_index = -1;
};

struct glTFMeshData
{
	Vector<float> vertices;   // interleaved: pos(3)+normal(3)+uv(2)=8 floats/vertex
	Vector<UInt32> indices;
	Vector<glTFPrimitive> primitives;
};

MYRENDERER_BEGIN_CLASS(glTFLoader)
#pragma region METHOD
public:
	static Bool METHOD(LoadFromFile)(const String& path, Vector<glTFMeshData>& out);
	static Bool METHOD(LoadFromMemory)(const UInt8* data, size_t size, Vector<glTFMeshData>& out);
protected:
private:
	static void METHOD(ProcessMesh)(tinygltf::Model& model, tinygltf::Mesh& mesh, glTFMeshData& out);
	static void METHOD(ProcessAccessor)(tinygltf::Model& model, Int accessor_idx, float* dst, Int stride, Int offset);
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE

#endif
