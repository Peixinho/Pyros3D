//============================================================================
// Name        : DeferredRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Deferred Renderer
//============================================================================

#ifndef DEFERREDRENDERER_H
#define DEFERREDRENDERER_H

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Rendering/Renderer/IRenderer.h>
#include <Pyros3D/Core/Projection/Projection.h>
#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>

namespace p3d {

	class PYROS3D_API DeferredRenderer : public IRenderer {

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
		FrameBuffer* FBO, *lastPassFBO;
		Texture* colorTexture;
		// A real *copy* of FBO's depth attachment, not an alias of it -
		// see IRenderDevice::CopyDepthTexture()'s comment for why:
		// lastPassFBO's lighting materials need to sample FBO's depth as
		// a plain texture (tDepth) while lastPassFBO's own later forward
		// sub-pass (transparent objects, drawn after the lighting
		// composite) needs a real depth *attachment* with matching values
		// - the same texture can't be both at once on Vulkan. Refreshed
		// once per frame in RenderScene(), right after FBO's G-buffer
		// pass ends and before lastPassFBO's lighting pass starts
		// sampling it.
		Texture* forwardDepthTexture;

		// Fallback shadow-comparison textures, bound to uShadowMap
		// whenever a light being drawn this frame *doesn't* cast a real
		// shadow. GL tolerates a declared-but-never-bound sampler just
		// fine (reads black, harmless); Vulkan requires every descriptor
		// a bound pipeline statically references to be valid the moment
		// it draws, even behind a runtime `uHaveShadowmap > 0.0` branch
		// the shader never actually takes - found via
		// VUID-vkCmdDrawIndexed-None-08114 on a real DeferredRendering
		// validation-layer run (100 non-shadow-casting point lights, so
		// uShadowMap's descriptor was never written at all). One 2D
		// (sampler2DShadow, shared by Directional/Spot) and one cube
		// (samplerCubeShadow, Point) - the descriptor's declared image
		// *type* has to match what's bound, a plain 2D fallback can't
		// stand in for samplerCubeShadow. Real shadow maps' contents
		// don't matter here since the shader never samples them behind
		// `uHaveShadowmap <= 0.0` - these are as small as
		// CreateEmptyTexture() allows.
		Texture* dummyShadow2D;
		Texture* dummyShadowCube;
		// True once dummyShadow2D/dummyShadowCube have been through a
		// real (contentless) render-pass begin/end - a freshly created
		// texture's backing image starts life in VK_IMAGE_LAYOUT_UNDEFINED
		// on Vulkan and only leaves it via an actual render pass or
		// upload, neither of which CreateEmptyTexture() alone triggers.
		// Deferred to RenderScene()'s first call (not the constructor)
		// so the Bind()/UnBind() pair below runs inside a real frame,
		// matching every other FBO bind in this class - found via
		// VUID-vkCmdDraw-None-09600 on a real DeferredRendering
		// validation-layer run (both dummies still VK_IMAGE_LAYOUT_UNDEFINED
		// the first time a light needed one).
		bool dummyShadowsWarmedUp;

		// Deferred Materials
		CustomShaderMaterial *deferredLastPass;
		CustomShaderMaterial *deferredMaterialAmbient;
		CustomShaderMaterial *deferredMaterialDirectional;
		CustomShaderMaterial *deferredMaterialPoint;
		CustomShaderMaterial *deferredMaterialSpot;
		RenderingComponent *directionalLight;
		RenderingComponent *pointLight;

		Renderable* sphereHandle;
		Renderable* quadHandle;

		// Uniform Handlers
		Uniform *pointPosHandle, *pointRadiusHandle, *pointColorHandle, *pointShadowHandle, *pointShadowDepthsMVPHandle, *pointShadowPCFTexelHandle, *pointHaveShadowHandle;
		Uniform *dirDirHandle, *dirColorHandle, *dirShadowHandle, *dirShadowPCFTexelHandle, *dirShadowDepthsMVPHandle, *dirShadowFarHandle, *dirHaveShadowHandle;
		Uniform *spotPosHandle, *spotDirHandle, *spotRadiusHandle, *spotOutterHandle, *spotInnerHandle, *spotColorHandle, *spotShadowHandle, *spotShadowDepthsMVPHandle, *spotShadowPCFTexelHandle, *spotHaveShadowHandle;
	};

};

#endif /* DEFERREDRENDERER_H */
