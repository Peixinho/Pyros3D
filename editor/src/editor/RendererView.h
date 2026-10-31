//============================================================================
// Name        : RendererView.h
// Author      : Duarte Peixinho
// Description : One of the editor's views drawn by the renderer (on the window's device):
//               a SceneRenderer of its own and the texture it draws into - the device's backbuffer format, the
//               view's size, made again when that changes - in the editor's frame, shown as an ImGui image. What a
//               DebugRenderer batched (a skeleton, a gizmo, a floor grid) is drawn over it, on top.
//============================================================================

#ifndef RENDERERVIEW2_H
#define RENDERERVIEW2_H

#include <Pyros3D/Rendering/Renderer/SceneRenderer.h>
#include <Pyros3D/Rendering/Renderer/ImGuiRenderer.h>
#include <Pyros3D/Rendering/Renderer/DebugRenderer/DebugRenderer.h>
#include <memory>

struct RendererView
{
	// (the editor's - Editor::Init sets them when its window is the device layer's)
	static p3d::gfx::Device* device;
	static p3d::ImGuiRenderer* imgui;
	static bool Active() { return device != nullptr; }

	std::unique_ptr<p3d::SceneRenderer> renderer;
	p3d::gfx::Texture target;
	uint32_t width = 0, height = 0;

	RendererView() {}
	RendererView(const RendererView&) = delete;
	RendererView& operator=(const RendererView&) = delete;
	~RendererView();

	// Its renderer, made on first use (to be given the scene's settings before Draw)
	p3d::SceneRenderer* Renderer();
	// The scene into its texture, as `view` says (its size, camera, projection), the debug batches over it; the texture as
	// ImGui shows it - NULL when there is no the device layer
	void* Draw(p3d::SceneGraph* scene, p3d::View view, p3d::DebugRenderer* debug = nullptr);
};

#include <memory>
namespace p3d { class Texture; }
// A texture asset as ImGui shows it: on the renderer, the device layer's copy of it (SceneRenderer::GpuTexture - a picture, sRGB);
// else the old device's (IRenderDevice::GetImGuiTextureID)
void* EditorImGuiTexture(p3d::Texture* texture);
inline void* EditorImGuiTexture(const std::shared_ptr<p3d::Texture> &texture) { return EditorImGuiTexture(texture.get()); }

#endif
