//============================================================================
// Name        : ImGuiRenderer.h
// Author      : Duarte Peixinho
// Description : Dear ImGui drawn on the device layer - the editor's panels over the renderer's views. ImGui 1.92's own
//               textures (its font atlas, made and grown as it asks: ImGuiBackendFlags_RendererHasTextures) are
//               the device layer textures; an ImTextureID is a device layer texture's id, so a view the renderer drew into
//               (View::target) is shown with ImGui::Image(ImGuiRenderer::Id(texture), size).
//============================================================================

#ifndef PYROS_RENDERER_IMGUIRENDERER2_H
#define PYROS_RENDERER_IMGUIRENDERER2_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Rendering/Device/RenderDevice.h>
#include <unordered_map>
#include <vector>

struct ImDrawData;
struct ImTextureData;

namespace p3d {

	class PYROS3D_API ImGuiRenderer
	{
	public:
		// (says to the current ImGui context that it draws it, and makes its textures)
		explicit ImGuiRenderer(gfx::Device* device);
		~ImGuiRenderer();

		// ImGui's frame into `target` (the window's backbuffer when none; either of the device's BackbufferFormat()),
		// `clear` first or over what is there. Its textures made, grown and let go as ImGui asks.
		void Render(ImDrawData* data, gfx::CommandList* cl, gfx::Texture target, const bool clear);

		// A device layer texture as ImGui shows it (sampleable, as a renderer view leaves its target)
		static unsigned long long Id(const gfx::Texture t) { return (unsigned long long)t.id; }
		// A texture of the host's destroyed (a view's target made again at a new size): its group let go
		void Forget(const gfx::Texture t);

	private:
		gfx::Device* device;
		gfx::Shader vs, fs;
		gfx::Pipeline pipeline;
		gfx::Sampler sampler;
		gfx::Buffer vertices, indices;
		uint64_t vertexBytes = 0, indexBytes = 0;     // (a frame slot's share of each)
		std::vector<uint8_t> staging;                 // (a frame's vertices, then its indices, gathered to be written at once)
		uint32_t slot = 0;
		static const uint32_t kSlots = 3;
		std::unordered_map<uint32_t, gfx::BindGroup> groups;     // (by texture id)
		void UpdateTexture(ImTextureData* tex);
		gfx::BindGroup GroupFor(const uint32_t textureId);
	};

}

#endif
