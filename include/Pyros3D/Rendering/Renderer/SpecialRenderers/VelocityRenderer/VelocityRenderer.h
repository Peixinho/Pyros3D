//============================================================================
// Name        : VelocityRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Dynamic Cube Map aka Environment Map
//============================================================================

#ifndef VRENDERER_H
#define VRENDERER_H

#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Core/Projection/Projection.h>

namespace p3d {

	class PYROS3D_API VelocityRenderer : public IRenderer {

	public:

		VelocityRenderer(const uint32 Width, const uint32 Height);

		~VelocityRenderer();
		
		void RenderVelocityMap(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene);

		void Resize(const uint32 &Width, const uint32 &Height);
		
		Texture* GetTexture();

	protected:
		
		virtual void RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene) {}

		GameObject* Camera;
		Texture* velocityMap;
		FrameBuffer* fbo;
		GenericShaderMaterial* velocityMaterial;

	};

};

#endif /* VRENDERER_H */
