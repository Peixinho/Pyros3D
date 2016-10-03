//============================================================================
// Name        : DeferredRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Deferred Renderer
//============================================================================

#ifndef DEFERREDRENDERER_H
#define DEFERREDRENDERER_H

#include "Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h"
#include "Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h"
#include "Pyros3D/Rendering/Renderer/IRenderer.h"
#include "Pyros3D/Core/Projection/Projection.h"
#include "Pyros3D/Core/Buffers/FrameBuffer.h"
#include "Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h"

namespace p3d {

	class DeferredRenderer : public IRenderer {

	public:

		DeferredRenderer(const uint32 Width, const uint32 Height, FrameBuffer* fbo);

		~DeferredRenderer();

		virtual void RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const uint32 BufferOptions = Buffer_Bit::Color | Buffer_Bit::Depth);

		void SetFBO(FrameBuffer* fbo);

		virtual void Resize(const uint32 Width, const uint32 Height);

	private:
		GenericShaderMaterial* shadowMaterial, *shadowSkinnedMaterial;

	protected:

		// Offscreen Frame Buffer Object
		FrameBuffer* FBO;

		// Point Light Volume
		CustomShaderMaterial *deferredMaterialDirectional;
		CustomShaderMaterial *deferredMaterialPoint;
		CustomShaderMaterial *deferredMaterialSpot;
		RenderingComponent *directionalLight;
		RenderingComponent *pointLight;

		Renderable* sphereHandle;
		Renderable* quadHandle;

		// Uniform Handlers
		Uniform *pointPosHandle, *pointRadiusHandle, *pointColorHandle;
		Uniform *dirDirHandle, *dirColorHandle;
		Uniform *spotPosHandle, *spotDirHandle, *spotRadiusHandle, *spotOutterHandle, *spotInnerHandle, *spotColorHandle;
	};

};

#endif /* DEFERREDRENDERER_H */
