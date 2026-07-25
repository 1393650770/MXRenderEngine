#pragma once
#ifndef _RENDERUTILS_
#define _RENDERUTILS_

#include "RenderEnum.h"
#include <memory>
#include <string>
#include "RenderRHI.h"
#include "RenderResource.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(RHI)

class Buffer;

MYRENDERER_TEMPLATE_HEAD(typename data_type, Bool is_keep_strong_references = false)
MYRENDERER_BEGIN_CLASS(MapHelper)
public:

	MapHelper()	MYDEFAULT;
	~MapHelper()
	{
		Unmap();
	}
	MapHelper(Buffer* in_buffer, ENUM_MAP_TYPE in_map_type, ENUM_MAP_FLAG in_map_flags) :
		MapHelper()
	{
		Map(in_buffer, in_map_type, in_map_flags);
	}

	MapHelper(MapHelper&& helper) :
		map_buffer{ std::move(helper.map_buffer) },
		map_data{ std::move(helper.map_data) },
		map_type{ std::move(helper.map_type) },
		map_flags{ std::move(helper.map_flags) }
	{
		helper.map_data = nullptr;
		helper.map_buffer = nullptr;
	}

	MapHelper& operator=(MapHelper&& helper)
	{
		map_buffer = std::move(helper.map_buffer);
		map_data = std::move(helper.map_data);
		map_type = std::move(helper.map_type);
		map_flags = std::move(helper.map_flags);
		helper.map_data = nullptr;
		helper.map_buffer = nullptr;
		return *this;
	}
	void METHOD(Map)(Buffer* in_buffer, ENUM_MAP_TYPE in_map_type, ENUM_MAP_FLAG in_map_flags)
	{
		CHECK_WITH_LOG(!(!map_buffer && !map_data), "Map Helper Error: Object already mapped!");
		Unmap();
		map_data = STATIC_CAST(RHIMapBuffer(in_buffer,in_map_type,in_map_flags), data_type);
		if (map_data != nullptr)
		{
			map_buffer = in_buffer;
			map_type = in_map_type;
			map_flags = in_map_flags;
		}
	}
	void METHOD(Unmap)()
	{
		if (map_buffer)
		{
			RHIUnmapBuffer(map_buffer);
			map_buffer = nullptr;
		}
		map_data = nullptr;
	}
	operator data_type* () { return map_data; }
	operator CONST data_type* () CONST { return map_data; }
	data_type* operator->() { return map_data; }
	CONST data_type* operator->() CONST { return map_data; }
protected:
	MapHelper(CONST MapHelper&) MYDEFAULT;
	MapHelper& operator=(CONST MapHelper&) MYDEFAULT;

	Buffer* map_buffer = nullptr;
	data_type* map_data=nullptr;

	ENUM_MAP_TYPE map_type=ENUM_MAP_TYPE::Read;

	ENUM_MAP_FLAG map_flags= ENUM_MAP_FLAG::None;


MYRENDERER_END_CLASS



MYRENDERER_BEGIN_CLASS_WITH_DERIVE(StreamingBuffer, public RenderResource)
#pragma region METHOD
public:
	StreamingBuffer(ENUM_BUFFER_TYPE in_buffer_type, UInt32 in_size, UInt32 in_num_contexts, CONST String in_name);
	VIRTUAL ~StreamingBuffer();

	UInt32  METHOD(Allocate)(UInt32 in_size, UInt32 ctx_num);

	void  METHOD(Release)(UInt32 ctx_num);

	void  METHOD(Flush)(UInt32 ctx_num);

	Buffer* METHOD(GetBuffer)();
	void* METHOD(GetMappedCPUAddress)(UInt32 ctx_num);

	void  METHOD(AllowPersistentMapping)(Bool AllowMapping);
protected:

private:

#pragma endregion


#pragma region MEMBER
public:

protected:
	BufferHandle buffer_handle;
	String name;
	Bool allow_persistent_map = false;
	MYRENDERER_BEGIN_STRUCT(MapInfo)
	public:
		MapHelper<UInt8> mapped_data;
		UInt32           curr_offset = 0;
	MYRENDERER_END_STRUCT
	
	Vector<MapInfo> map_infos;

private:
#pragma endregion



MYRENDERER_END_CLASS


MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
#endif //_RENDERUTILS_
