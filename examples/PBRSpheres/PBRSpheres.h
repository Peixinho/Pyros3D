//============================================================================
// Name        : PBRSpheres.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Metallic/roughness PBR calibration grid - forward-rendering
//                only (see VULKAN_ROADMAP.md / the PBR plan for scope). A
//                grid of spheres shares one albedo, varying SetMetallic()
//                0->1 along X and SetRoughness() 0->1 along Y, plus one
//                extra sphere exercising SetMetallicRoughnessMap() (the
//                PBRMap/binding-16 path) with a small procedural gradient
//                texture instead of a shared metallic/roughness pair.
//============================================================================

#ifndef PBRSPHERES_H
#define	PBRSPHERES_H

#include "../BaseExample/BaseExample.h"

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <vector>

using namespace p3d;

class PBRSpheres : public BaseExample {

public:

	PBRSpheres();
	virtual ~PBRSpheres();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	static const uint32 GRID = 5;

	ForwardRenderer* Renderer;
	Projection projection;

	GameObject* Light;
	DirectionalLight* dLight;

	Renderable* sphereMesh;

	// Grid: metallic varies 0->1 along columns, roughness 0->1 along rows.
	GameObject* gridObjs[GRID][GRID];
	RenderingComponent* rGrid[GRID][GRID];
	GenericShaderMaterial* gridMaterials[GRID][GRID];

	// One extra sphere off to the side, using SetMetallicRoughnessMap()
	// instead of scalar Set Metallic()/SetRoughness() - exercises the
	// PBRMap/binding-16 sampler path specifically.
	GameObject* mapObj;
	RenderingComponent* rMap;
	GenericShaderMaterial* mapMaterial;
	Texture* metallicRoughnessTexture;

};

#endif	/* PBRSPHERES_H */
