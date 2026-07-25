#pragma once
#ifndef _GLTF_LOADER_
#define _GLTF_LOADER_

// glTF 2.0 Runtime Loader — Adapter pattern (tinygltf → MeshDataPayload).
// Requires ThirdParty/tinygltf/tiny_gltf.h (header-only, MIT license).
// Pull: cd src/ThirdParty && git clone https://github.com/syoyo/tinygltf.git --depth 1

#include "Core/ConstDefine.h"

// TODO: uncomment when tinygltf is pulled
// #define TINYGLTF_NO_INCLUDE_STB_IMAGE
// #define TINYGLTF_NO_STB_IMAGE_WRITE
// #include "tinygltf/tiny_gltf.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Asset)
MYRENDERER_BEGIN_NAMESPACE(glTF)

struct glTFPrimitive
{
	UInt32 index_offset = 0;
	UInt32 index_count = 0;
	UInt32 vertex_offset = 0;
	UInt32 vertex_count = 0;
	Int material_index = -1;
};

struct glTFMeshData
{
	Vector<float> vertices;          // interleaved: pos(3) + normal(3) + uv(2) = 8 floats
	Vector<UInt32> indices;
	Vector<glTFPrimitive> primitives;
};

MYRENDERER_BEGIN_CLASS(glTFLoader)
#pragma region METHOD
public:
	// Load from file (requires AsyncFileIO or sync PlatformFileIO)
	// static Bool LoadFromFile(const String& path, Vector<glTFMeshData>& out);

	// Load from memory buffer (.gltf or .glb)
	// static Bool LoadFromMemory(const UInt8* data, size_t size, Vector<glTFMeshData>& out);

	// Full implementation in glTFLoader.cpp — requires tinygltf header
protected:
private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // glTF
MYRENDERER_END_NAMESPACE  // Asset
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _GLTF_LOADER_
