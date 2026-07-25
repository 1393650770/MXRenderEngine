#include "stb_image/stb_image.h"  // stbi declarations (impl in stb_image.cpp)
#define TINYGLTF_IMPLEMENTATION
#include "Asset/glTF/glTFLoader.h"
#include <cstring>
#include <iostream>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(Asset)
MYRENDERER_BEGIN_NAMESPACE(glTF)

Bool glTFLoader::LoadFromFile(const String& path, Vector<glTFMeshData>& out)
{
	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	String err, warn;

	Bool is_binary = (path.size() > 4 && path.substr(path.size()-4) == ".glb");
	Bool ok = is_binary
		? loader.LoadBinaryFromFile(&model, &err, &warn, path)
		: loader.LoadASCIIFromFile(&model, &err, &warn, path);

	if (!warn.empty()) std::cout << "[glTF] Warning: " << warn << std::endl;
	if (!err.empty())  std::cout << "[glTF] Error: " << err << std::endl;
	if (!ok) return false;

	out.resize(model.meshes.size());
	for (size_t i = 0; i < model.meshes.size(); ++i)
		ProcessMesh(model, model.meshes[i], out[i]);

	return true;
}

Bool glTFLoader::LoadFromMemory(const UInt8* data, size_t size, Vector<glTFMeshData>& out)
{
	if (!data || size < 4) return false;

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	String err, warn;

	Bool is_binary = (data[0] == 'g' && data[1] == 'l' && data[2] == 'T' && data[3] == 'F');
	Bool ok = is_binary
		? loader.LoadBinaryFromMemory(&model, &err, &warn, data, size)
		: loader.LoadASCIIFromString(&model, &err, &warn, reinterpret_cast<const char*>(data), (unsigned int)size, "", 0);

	if (!ok) return false;

	out.resize(model.meshes.size());
	for (size_t i = 0; i < model.meshes.size(); ++i)
		ProcessMesh(model, model.meshes[i], out[i]);

	return true;
}

void glTFLoader::ProcessMesh(tinygltf::Model& model, tinygltf::Mesh& mesh, glTFMeshData& out)
{
	for (auto& prim : mesh.primitives)
	{
		glTFPrimitive p;
		p.index_offset = (UInt32)out.indices.size();
		p.material_index = prim.material;

		// Read indices
		if (prim.indices >= 0)
		{
			auto& acc = model.accessors[prim.indices];
			auto& bv = model.bufferViews[acc.bufferView];
			auto& buf = model.buffers[bv.buffer];
			const UInt8* src = buf.data.data() + bv.byteOffset + acc.byteOffset;

			p.index_count = (UInt32)acc.count;
			size_t idx_start = out.indices.size();
			out.indices.resize(idx_start + acc.count);

			for (size_t i = 0; i < acc.count; ++i)
			{
				UInt32 idx = 0;
				if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
					idx = reinterpret_cast<const UInt16*>(src)[i];
				else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
					idx = reinterpret_cast<const UInt32*>(src)[i];
				else if (acc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
					idx = src[i];
				out.indices[idx_start + i] = idx;
			}
		}

		// Read position (required)
		Int pos_acc = -1, normal_acc = -1, uv_acc = -1;
		for (auto& attr : prim.attributes)
		{
			if (attr.first == "POSITION") pos_acc = attr.second;
			else if (attr.first == "NORMAL") normal_acc = attr.second;
			else if (attr.first == "TEXCOORD_0") uv_acc = attr.second;
		}

		if (pos_acc < 0) continue;

		auto& pos_accessor = model.accessors[pos_acc];
		UInt32 vertex_count = (UInt32)pos_accessor.count;
		p.vertex_offset = (UInt32)(out.vertices.size() / 8);
		p.vertex_count = vertex_count;

		// Interleaved vertex: pos(3) + normal(3) + uv(2) = 8 floats
		size_t vert_start = out.vertices.size();
		out.vertices.resize(vert_start + vertex_count * 8, 0.0f);

		// POSITION
		ProcessAccessor(model, pos_acc, out.vertices.data() + vert_start, 8, 0);

		// NORMAL (default up=0,1,0)
		if (normal_acc >= 0)
			ProcessAccessor(model, normal_acc, out.vertices.data() + vert_start, 8, 3);
		else
			for (UInt32 i = 0; i < vertex_count; ++i)
				{ out.vertices[vert_start + i*8 + 3] = 0; out.vertices[vert_start + i*8 + 4] = 1; out.vertices[vert_start + i*8 + 5] = 0; }

		// TEXCOORD_0
		if (uv_acc >= 0)
			ProcessAccessor(model, uv_acc, out.vertices.data() + vert_start, 8, 6);

		out.primitives.push_back(p);
	}
}

void glTFLoader::ProcessAccessor(tinygltf::Model& model, Int accessor_idx, float* dst, Int stride, Int offset)
{
	auto& acc = model.accessors[accessor_idx];
	auto& bv = model.bufferViews[acc.bufferView];
	auto& buf = model.buffers[bv.buffer];
	const UInt8* src = buf.data.data() + bv.byteOffset + acc.byteOffset;
	Int comp_count = tinygltf::GetNumComponentsInType(acc.type);

	if (acc.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
	{
		const float* fsrc = reinterpret_cast<const float*>(src);
		for (size_t i = 0; i < acc.count; ++i)
			for (Int j = 0; j < comp_count; ++j)
				dst[i * stride + offset + j] = fsrc[i * comp_count + j];
	}
}

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
