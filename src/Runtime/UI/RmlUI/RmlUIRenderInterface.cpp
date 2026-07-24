#include "RmlUIRenderInterface.h"
#include "RmlUIRenderer.h"

#include <RmlUi/Core/RenderInterface.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Vertex.h>
#include <RmlUi/Core/Types.h>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(UI)
MYRENDERER_BEGIN_NAMESPACE(RmlUI)

// RmlUI raw UInt32 → typed Handle conversion helpers
static UIGeometryHandle ToGeo(UInt32 raw) { UIGeometryHandle h; h.value = raw; return h; }
static UITextureHandle  ToTex(UInt32 raw) { UITextureHandle  h; h.value = raw; return h; }

class RmlUIRenderInterface::Impl : public Rml::RenderInterface
{
public:
	RmlUIRenderer* renderer = nullptr;
	explicit Impl(RmlUIRenderer* r) : renderer(r) {}

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override
	{
		if (!renderer) return 0;
		auto h = renderer->CompileGeometry(
			vertices.data(), static_cast<UInt32>(vertices.size()), sizeof(Rml::Vertex),
			indices.data(), static_cast<UInt32>(indices.size()), true);
		return static_cast<Rml::CompiledGeometryHandle>(h.value);
	}

	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation, Rml::TextureHandle texture) override
	{
		if (!renderer) return;
		renderer->SetTranslation(translation.x, translation.y);
		renderer->DrawGeometry(ToGeo(static_cast<UInt32>(geometry)),
			nullptr, ToTex(static_cast<UInt32>(texture)));
	}

	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override
	{
		if (!renderer) return;
		renderer->ReleaseGeometry(ToGeo(static_cast<UInt32>(geometry)));
	}

	Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override
	{
		if (!renderer) return 0;
		auto* file_iface = Rml::GetFileInterface();
		if (!file_iface) return 0;
		auto fh = file_iface->Open(source);
		if (!fh) return 0;
		size_t total_size = file_iface->Length(fh);
		Rml::UniquePtr<Rml::byte[]> data = Rml::MakeUnique<Rml::byte[]>(total_size);
		file_iface->Read(data.get(), total_size, fh);
		file_iface->Close(fh);
		if (total_size < 18) return 0;
		const Rml::byte* buf = data.get();
		if (buf[2] != 2) return 0;
		int w = buf[12] | (buf[13] << 8);
		int h = buf[14] | (buf[15] << 8);
		int bpp = buf[16];
		if (bpp != 24 && bpp != 32) return 0;
		int pixel_bytes = bpp / 8;
		int data_offset = 18 + buf[0];
		const Rml::byte* pixel_buf = buf + data_offset;
		Rml::Vector<Rml::byte> rgba8;
		rgba8.resize(static_cast<size_t>(w) * h * 4);
		for (int i = 0; i < w * h; i++)
		{
			int si = i * pixel_bytes, di = i * 4;
			rgba8[di+0] = pixel_buf[si+2]; rgba8[di+1] = pixel_buf[si+1];
			rgba8[di+2] = pixel_buf[si+0]; rgba8[di+3] = (bpp == 24) ? 255 : pixel_buf[si+3];
		}
		texture_dimensions.x = w; texture_dimensions.y = h;
		auto hdl = renderer->CreateTexture(rgba8.data(), static_cast<UInt32>(w), static_cast<UInt32>(h));
		return static_cast<Rml::TextureHandle>(hdl.value);
	}

	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override
	{
		if (!renderer) return 0;
		auto hdl = renderer->CreateTexture(source.data(),
			static_cast<UInt32>(source_dimensions.x), static_cast<UInt32>(source_dimensions.y));
		return static_cast<Rml::TextureHandle>(hdl.value);
	}

	void ReleaseTexture(Rml::TextureHandle texture) override
	{
		if (!renderer) return;
		UITextureHandle h; h.value = static_cast<UInt32>(texture);
		renderer->ReleaseTexture(h);
	}

	void EnableScissorRegion(bool enable) override { if (renderer) renderer->EnableScissor(enable); }
	void SetScissorRegion(Rml::Rectanglei region) override
	{
		if (!renderer) return;
		renderer->SetScissor(region.Left(), region.Top(),
			static_cast<UInt32>(region.Width()), static_cast<UInt32>(region.Height()));
	}

	void EnableClipMask(bool enable) override { if (renderer) renderer->EnableClipMask(enable); }
	void RenderToClipMask(Rml::ClipMaskOperation operation, Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation) override
	{
		if (!renderer) return;
		renderer->SetTranslation(translation.x, translation.y);
		renderer->RenderToClipMask(static_cast<Int>(operation),
			ToGeo(static_cast<UInt32>(geometry)), nullptr);
	}

	void SetTransform(const Rml::Matrix4f* transform) override
	{
		if (!renderer) return;
		renderer->SetTransform(transform ? transform->data() : nullptr);
	}
};

RmlUIRenderInterface::RmlUIRenderInterface(RmlUIRenderer* renderer) { m_impl = new Impl(renderer); }
RmlUIRenderInterface::~RmlUIRenderInterface() { delete m_impl; m_impl = nullptr; }
void* RmlUIRenderInterface::GetRmlInterface() CONST { return static_cast<Rml::RenderInterface*>(m_impl); }
void RmlUIRenderInterface::SetRenderer(RmlUIRenderer* renderer) { if (m_impl) m_impl->renderer = renderer; }

MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
MYRENDERER_END_NAMESPACE
