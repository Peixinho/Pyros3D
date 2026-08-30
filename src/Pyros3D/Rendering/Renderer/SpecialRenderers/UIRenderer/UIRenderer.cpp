//============================================================================
// Name        : UIRenderer
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Screen-space pass that draws a scene's UI canvases
//============================================================================

#include <Pyros3D/Rendering/Renderer/SpecialRenderers/UIRenderer/UIRenderer.h>
#include <Pyros3D/Rendering/Components/UI/UICanvas.h>
#include <Pyros3D/GameObjects/GameObject.h>

namespace p3d {

	UIRenderer::UIRenderer(const uint32 Width, const uint32 Height) : IRenderer(Width, Height)
	{
		echo("SUCCESS: UI Renderer Created");

		// Only UI meshes, and nothing else sees them - see RenderLayer in
		// RenderingComponent.h.
		SetRenderLayer(RenderLayer::UI);

		uiCamera = std::make_shared<GameObject>();
		uiCamera->RefreshTransformation();

		viewPortStartX = viewPortStartY = 0;
		viewPortEndX = viewPortEndY = 0;
	}

	UIRenderer::~UIRenderer() {}

	void UIRenderer::Resize(const uint32 &Width, const uint32 &Height)
	{
		IRenderer::Resize(Width, Height);
		// And the viewport with it. RenderUI() only falls back to
		// Width/Height while these are still zero, so without this a
		// renderer that was resized once kept drawing every canvas at the
		// size it was first used at - which for an editor viewport is every
		// size but the current one.
		viewPortStartX = viewPortStartY = 0;
		viewPortEndX = Width;
		viewPortEndY = Height;
	}

	void UIRenderer::RenderUI(SceneGraph* Scene)
	{
		if (Scene == NULL) return;

		std::vector<UICanvas*> canvases = UICanvas::GetCanvasesOnScene(Scene);
		if (canvases.empty()) return;

		this->Scene = Scene;
		this->Camera = uiCamera.get();
		Timer = Scene->GetTime();

		InitRender();

		if (viewPortEndX == 0 || viewPortEndY == 0)
		{
			viewPortEndX = Width;
			viewPortEndY = Height;
		}
		_SetViewPort(viewPortStartX, viewPortStartY, viewPortEndX, viewPortEndY);

		for (size_t c = 0; c < canvases.size(); c++)
		{
			UICanvas* canvas = canvases[c];
			canvas->Solve(viewPortEndX - viewPortStartX, viewPortEndY - viewPortStartY);

			const UIRectValue &rect = canvas->GetCanvasRect();
			if (rect.width <= 0.f || rect.height <= 0.f) continue;

			// Canvas point (x, y) lives at (x, -y) in the canvas
			// GameObject's space - see UIRectValue's comment - so the ortho
			// box is x in [0, width] and y in [-height, 0]. The depth range
			// is wide and symmetric because nothing here depth-tests; it
			// only has to not clip.
			projection.Ortho(0.f, rect.width, -rect.height, 0.f, -1000.f, 1000.f);

			PrvProjectionMatrix = ProjectionMatrix;
			ProjectionMatrix = projection.m;
			NearFarPlane = Vec2(projection.Near, projection.Far);

			PrvViewMatrix = ViewMatrix;
			ViewMatrix = uiCamera->GetWorldTransformation().Inverse();
			CameraPosition = uiCamera->GetWorldPosition();

			ProjectionMatrixInverseIsDirty = true;
			ViewMatrixInverseIsDirty = true;
			ViewProjectionMatrixIsDirty = true;

			// Straight down the canvas's own list, in order, no sorting -
			// see the class comment.
			const std::vector<RenderingMesh*> &list = canvas->GetBatchedDrawList();
			const std::vector<UIRectValue> &clips = canvas->GetBatchedDrawClips();
			const f32 ppu = canvas->GetPixelsPerUnit();
			// Only when it changes: most of a canvas is unclipped, and the
			// scissor is device state, not per-draw data.
			bool scissorOn = false;
			UIRectValue lastClip;
			for (size_t i = 0; i < list.size(); i++)
			{
				RenderingMesh* m = list[i];
				if (m == NULL || m->renderingComponent == NULL) continue;
				if (!m->renderingComponent->IsActive() || !m->Active) continue;
				if (m->renderingComponent->GetOwner() == NULL) continue;
				if (!m->Material) continue;
				// An empty label builds a real mesh with no quads in it.
				// Nothing to draw, and nothing sane to bind either.
				if (m->Geometry == NULL || m->Geometry->GetIndexData().empty()) continue;

				if (i < clips.size())
				{
					const UIRectValue &c = clips[i];
					// The whole canvas means no clipping, which is what
					// almost everything gets - and turning the scissor off
					// costs less than a scissor covering everything.
					const bool needed = !(c.x <= 0.f && c.y <= 0.f &&
						c.width >= rect.width && c.height >= rect.height);
					if (!needed)
					{
						if (scissorOn) { GetActiveRenderDevice().SetScissorTestEnabled(false); scissorOn = false; }
					}
					else if (!scissorOn || c.x != lastClip.x || c.y != lastClip.y ||
						c.width != lastClip.width || c.height != lastClip.height)
					{
						// Canvas units to pixels, top-left origin, which is
						// what IRenderDevice::SetScissorRect takes on every
						// backend.
						GetActiveRenderDevice().SetScissorRect(
							(f32)viewPortStartX + c.x * ppu, (f32)viewPortStartY + c.y * ppu,
							c.width * ppu, c.height * ppu);
						GetActiveRenderDevice().SetScissorTestEnabled(true);
						scissorOn = true;
						lastClip = c;
					}
					// Nothing survives an empty clip, so skip the draw
					// outright rather than asking the GPU to discard it.
					if (needed && (c.width <= 0.f || c.height <= 0.f)) continue;
				}

				RenderObject(m, m->renderingComponent->GetOwner(), m->Material.get());
			}
		}

		// Never leave the scissor narrowed: the next pass through this
		// device knows nothing about this canvas's clipping.
		GetActiveRenderDevice().SetScissorTestEnabled(false);

		EndRender();
	}

};
