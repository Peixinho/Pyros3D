//============================================================================
// Name        : UIRenderer
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Screen-space pass that draws a scene's UI canvases
//============================================================================

#ifndef UIRENDERER_H
#define UIRENDERER_H

#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <memory>

namespace p3d {

	// Draws every UICanvas in a scene, in canvas units, over whatever is
	// already in the current render target. It owns no framebuffer: the
	// point is to composite on top of the finished 3D frame, and both
	// Forward and Deferred leave that in the bound target by the time this
	// runs.
	//
	// A separate IRenderer rather than a second RenderScene() call on the
	// main one, for three reasons that are each independently sufficient:
	// PreRender()/RenderScene() do nothing at all when a scene has no lights
	// (AxisHelper carries a dummy directional light purely to work around
	// that); UI wants none of the culling, shadow or lighting work those
	// paths do; and UI draw order is the canvas hierarchy, not distance from
	// a camera, so the translucency sort in GroupAndSortAssets() is actively
	// wrong here. This walks UICanvas::GetDrawList() instead and sorts
	// nothing.
	class PYROS3D_API UIRenderer : public IRenderer {

	public:

		UIRenderer(const uint32 Width, const uint32 Height);
		virtual ~UIRenderer();

		// Solves every canvas in the scene for the current viewport and
		// draws them in ascending sort order.
		void RenderUI(SceneGraph* Scene);

		virtual void Resize(const uint32 &Width, const uint32 &Height);

	protected:

		// Not a general-purpose scene renderer - RenderUI() is the entry
		// point, same shape as VelocityRenderer.
		virtual void RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene) {}

		// Identity camera. Canvas space IS the canvas GameObject's local
		// space, so the view matrix is identity and any transform on the
		// canvas object itself (a screen shake, a menu slide) composes
		// through the normal model matrix.
		std::shared_ptr<GameObject> uiCamera;
	};

};

#endif /* UIRENDERER_H */
