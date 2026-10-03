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
		// The velocity pass's own depth, unjittered. 1.0 wherever no mesh
		// was drawn - sky and background, which have no velocity written.
		Texture* GetDepthTexture() { return depthMap; }
		// prevVP * inverse(currentVP), from the matrices this pass just
		// drew with. Identity until a second frame exists. Motion blur
		// reprojects the sky with it, because nothing writes the sky's
		// velocity into the map.
		const Matrix &GetCameraReproject() const { return cameraReproject; }

	protected:
		
		virtual void RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene) {}

		GameObject* Camera;
		Texture* velocityMap;
		Texture* depthMap;
		FrameBuffer* fbo;
		GenericShaderMaterial* velocityMaterial;
		// This renderer's own matrices, not IRenderer's shared ones: the UI
		// pass and the other renderer both overwrite those before we draw.
		bool havePrevious = false;
		Matrix previousProjection, previousView;
		Matrix cameraReproject;

	};

};

#endif /* VRENDERER_H */
