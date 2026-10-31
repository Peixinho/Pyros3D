//============================================================================
// Name        : RendererView.cpp
// Author      : Duarte Peixinho
// Description : One of the editor's views drawn by the renderer - see RendererView.h
//============================================================================

#include "RendererView.h"
#include <Pyros3D/Assets/Texture/Texture.h>

using namespace p3d;

p3d::gfx::Device* RendererView::device = nullptr;
p3d::ImGuiRenderer* RendererView::imgui = nullptr;

RendererView::~RendererView()
{
	if (target && device) { if (imgui) imgui->Forget(target); device->Destroy(target); }
}

p3d::SceneRenderer* RendererView::Renderer()
{
	if (!renderer && device) renderer.reset(new p3d::SceneRenderer(device));
	return renderer.get();
}

void* RendererView::Draw(p3d::SceneGraph* scene, p3d::View view, p3d::DebugRenderer* debug)
{
	if (!device || !scene || view.width == 0 || view.height == 0) return nullptr;
	Renderer();
	if (!target || view.width != width || view.height != height)
	{
		if (target) { if (imgui) imgui->Forget(target); device->Destroy(target); }
		p3d::gfx::TextureDesc d; d.width = view.width; d.height = view.height; d.format = device->BackbufferFormat();
		d.usage = p3d::gfx::TexColorTarget | p3d::gfx::TexSampled | p3d::gfx::TexCopySrc; d.debugName = "Editor.View";
		target = device->CreateTexture(d);
		width = view.width; height = view.height;
	}
	if (debug)
	{
		// (what it batched, on top: depth-tested off, as the old DebugRenderer drew it)
		std::vector<Vec3> lines, triangles; std::vector<Vec4> lineColors, triangleColors;
		debug->TakeBatches(lines, lineColors, triangles, triangleColors);
		typedef p3d::SceneRenderer::OverlayVertex OV;
		auto pack = [](const std::vector<Vec3> &p, const std::vector<Vec4> &c) {
			std::vector<OV> v(p.size());
			for (size_t i = 0; i < p.size(); i++)
			{
				v[i].position[0] = p[i].x; v[i].position[1] = p[i].y; v[i].position[2] = p[i].z;
				v[i].color[0] = c[i].x; v[i].color[1] = c[i].y; v[i].color[2] = c[i].z; v[i].color[3] = c[i].w;
			}
			return v;
		};
		if (!triangles.empty()) renderer->AddOverlay(pack(triangles, triangleColors), false, false);
		if (!lines.empty()) renderer->AddOverlay(pack(lines, lineColors), true, false);
	}
	view.target = target; view.hostFrame = true;     // (the editor's frame: begun by it, its panels over this, presented by it)
	renderer->Render(scene, view);
	return (void*)(uintptr_t)p3d::ImGuiRenderer::Id(target);
}

void* EditorImGuiTexture(p3d::Texture* texture)
{
	if (!texture) return nullptr;
	// (one renderer for them all, drawing nothing: its texture cache)
	static RendererView bridge;
	if (!bridge.renderer) bridge.Renderer()->SetName("Editor images (textures)");
	const p3d::gfx::Texture t = bridge.Renderer()->GpuTexture(texture, true);
	return t ? (void*)(uintptr_t)p3d::ImGuiRenderer::Id(t) : nullptr;
}
