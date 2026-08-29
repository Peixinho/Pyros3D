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
			const std::vector<RenderingMesh*> &list = canvas->GetDrawList();
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
				RenderObject(m, m->renderingComponent->GetOwner(), m->Material.get());
			}
		}

		EndRender();
	}

};
