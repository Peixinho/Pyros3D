//============================================================================
// Name        : SceneView.cpp
// Author      : Duarte Peixinho
// Description : A second view of a scene, drawn into a texture a material shows
//============================================================================

#include <Pyros3D/Rendering/Renderer/SceneView.h>
#include <Pyros3D/Rendering/Renderer/SceneRenderer.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <algorithm>

namespace p3d {

	namespace { SceneRenderer* mainRenderer = nullptr; }
	void SceneView::SetMainRenderer(SceneRenderer* main) { mainRenderer = main; }
	SceneRenderer* SceneView::MainRenderer()
	{
		const std::vector<SceneRenderer*> &live = SceneRenderer::Live();
		if (mainRenderer && std::find(live.begin(), live.end(), mainRenderer) != live.end()) return mainRenderer;
		return live.empty() ? nullptr : live.front();
	}

	SceneView::SceneView(gfx::Device* d, const uint32 w, const uint32 h) : device(d), renderer(new SceneRenderer(d)), width(std::max(w, 1u)), height(std::max(h, 1u)),
		texture(new Texture())
	{
		renderer->SetName("Scene view");
		MakeTarget();
	}

	SceneView::~SceneView()
	{
		SceneRenderer::SetViewTexture(texture.get(), gfx::Texture());
		if (target) device->Destroy(target);
	}

	void SceneView::MakeTarget()
	{
		if (target) { SceneRenderer::SetViewTexture(texture.get(), gfx::Texture()); device->Destroy(target); }
		// (the device's picture format - what the renderer's last pass writes; an sRGB one where the device's is, so a
		// material reads its colours back as light)
		gfx::TextureDesc t; t.width = width; t.height = height; t.format = device->BackbufferFormat();
		t.usage = gfx::TexColorTarget | gfx::TexSampled; t.debugName = "SceneView";
		target = device->CreateTexture(t);
		// (its size said on the texture too: what shows it - a UI image's nine-slice, its aspect - asks it)
		texture->Resize(width, height);
		SceneRenderer::SetViewTexture(texture.get(), target);
	}

	void SceneView::Resize(const uint32 w, const uint32 h)
	{
		if (std::max(w, 1u) == width && std::max(h, 1u) == height) return;
		width = std::max(w, 1u); height = std::max(h, 1u);
		MakeTarget();
	}

	void SceneView::Render(SceneGraph* scene, GameObject* camera, const Projection &projection)
	{
		if (!scene || !target) return;
		// (the look of the view it was made beside, where that one is still drawing)
		if (follow)
		{
			const std::vector<SceneRenderer*> &live = SceneRenderer::Live();
			if (std::find(live.begin(), live.end(), follow) != live.end()) renderer->CopySettingsFrom(*follow);
			else follow = nullptr;
		}
		if (ownBackground) renderer->SetBackground(background);
		// (the frame it is part of: begun here where nothing has begun it yet - the frame's own view, after, adds to it)
		device->BeginFrame();
		View v;
		if (camera) v.cameraWorld = camera->GetDrawWorld();
		// (perspective or orthographic, told apart by the matrix: the fields the other one sets are left as they were)
		if (projection.m.m[15] == 1.f)
		{
			v.orthographic = true;
			v.orthoHeight = std::max(projection.Top - projection.Bottom, 0.001f);
			v.nearZ = projection.Near; v.farZ = projection.Far;
		}
		else
		{
			v.fovY = projection.Fov > 0.f ? projection.Fov : 70.f;
			v.nearZ = projection.Near > 0.f ? projection.Near : 0.1f;
		}
		v.width = width; v.height = height;
		v.target = target; v.hostFrame = true; v.ui = false; v.transparent = transparent;
		renderer->Render(scene, v);
	}

}
