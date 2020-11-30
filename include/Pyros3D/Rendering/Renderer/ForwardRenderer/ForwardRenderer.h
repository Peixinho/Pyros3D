//============================================================================
// Name        : ForwardRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Forward Renderer
//============================================================================

#ifndef FORWARDRENDERER_H
#define FORWARDRENDERER_H

#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Core/Projection/Projection.h>

namespace p3d {

	class PYROS3D_API ForwardRenderer : public IRenderer {

	public:

		ForwardRenderer(const uint32 Width, const uint32 Height);

		~ForwardRenderer();
		
		virtual void RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene);

	};

};

#endif /* FORWARDRENDERER_H */
