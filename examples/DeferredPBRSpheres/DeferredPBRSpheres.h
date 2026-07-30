//============================================================================
// Name        : DeferredPBRSpheres.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Metallic/roughness PBR calibration grid rendered through
//                DeferredRenderer - the deferred counterpart to
//                examples/PBRSpheres (forward-only). Built as a dedicated
//                example rather than reusing examples/DeferredRendering,
//                since that example's camera sits far from a scattering of
//                100 tiny, randomly-placed lights/cubes - not a useful
//                layout for actually seeing per-sphere metallic/roughness
//                response. This one keeps the camera close and uses a
//                handful of deliberately-placed point lights instead.
//============================================================================

#ifndef DEFERREDPBRSPHERES_H
#define	DEFERREDPBRSPHERES_H

#include "../BaseExample/BaseExample.h"

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <vector>

using namespace p3d;

class DeferredPBRSpheres : public BaseExample {

public:

	DeferredPBRSpheres();
	virtual ~DeferredPBRSpheres();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	static const uint32 GRID = 4;
	static const uint32 NUM_LIGHTS = 4;

	DeferredRenderer* Renderer;
	Projection projection;

	Renderable* sphereMesh;

	// Grid: metallic varies 0->1 along columns, roughness 0->1 along rows.
	GameObject* gridObjs[GRID][GRID];
	RenderingComponent* rGrid[GRID][GRID];
	GenericShaderMaterial* gridMaterials[GRID][GRID];

	GameObject* lightObjs[NUM_LIGHTS];
	PointLight* pointLights[NUM_LIGHTS];

	// G-buffer (4 color attachments + depth - see DeferredRenderer's
	// caller-constructed-FBO convention).
	Texture *albedoTexture, *specularTexture, *depthTexture, *normalTexture;
	Texture *metallicRoughnessTexture;
	FrameBuffer* deferredFBO;

};

#endif	/* DEFERREDPBRSPHERES_H */
