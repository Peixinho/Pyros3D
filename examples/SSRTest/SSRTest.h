//============================================================================
// Name        : SSRTest.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Material-aware screen-space reflection verification - a
//                low-roughness floor beneath a handful of PBR spheres of
//                varying roughness/metallic, viewed at an angle so both
//                the spheres and their floor reflections are visible at
//                once. Reuses DeferredPBRSpheres' proven G-buffer/
//                DeferredRenderer setup pattern; the floor is what that
//                example has no reason to include (a flat calibration
//                grid has nothing to reflect off of).
//============================================================================

#ifndef SSRTEST_H
#define	SSRTEST_H

#include "../BaseExample/BaseExample.h"

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/PostEffects/Effects/TonemapEffect.h>
#include <memory>

using namespace p3d;

class SSRTest : public BaseExample {

public:

	SSRTest();
	virtual ~SSRTest();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	static const uint32 NUM_SPHERES = 5;
	static const uint32 NUM_LIGHTS = 2;

	DeferredRenderer* Renderer;
	Projection projection;

	PostEffectsManager* EffectManager;

	std::shared_ptr<Renderable> sphereMesh;
	std::shared_ptr<Renderable> floorMesh;

	std::shared_ptr<GameObject> sphereObjs[NUM_SPHERES];
	std::shared_ptr<RenderingComponent> rSpheres[NUM_SPHERES];
	std::shared_ptr<GenericShaderMaterial> sphereMaterials[NUM_SPHERES];

	std::shared_ptr<GameObject> floorObj;
	std::shared_ptr<RenderingComponent> rFloor;
	std::shared_ptr<GenericShaderMaterial> floorMaterial;

	std::shared_ptr<GameObject> lightObjs[NUM_LIGHTS];
	std::shared_ptr<PointLight> pointLights[NUM_LIGHTS];
	std::shared_ptr<GameObject> dirLightObj;
	std::shared_ptr<DirectionalLight> dirLight;

	// G-buffer (4 color attachments + depth - see DeferredRenderer's
	// caller-constructed-FBO convention). FrameBuffer still takes raw
	// Texture* (IslandDemo pattern: own via shared_ptr, pass .get()).
	std::shared_ptr<Texture> albedoTexture, specularTexture, depthTexture, normalTexture;
	std::shared_ptr<Texture> metallicRoughnessTexture;
	FrameBuffer* deferredFBO;

};

#endif	/* SSRTEST_H */
