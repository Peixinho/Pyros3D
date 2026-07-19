//============================================================================
// Name        : LightPriorityExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Demonstrates per-object nearest-light selection when more
//               point lights are relevant to an object than the shader's
//               MAX_LIGHTS (4) cap can hold.
//============================================================================

#ifndef LIGHTPRIORITYEXAMPLE_H
#define	LIGHTPRIORITYEXAMPLE_H

#include "../BaseExample/BaseExample.h"

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>

using namespace p3d;

// Six point lights, each a distinct color, laid out on a line 200 units
// apart. Three test cubes sit between them. Every light's radius covers the
// whole row, so which lights end up affecting a given cube is decided
// entirely by the nearest-4 sort in ForwardRenderer::RenderScene, not by
// falloff cutoff. Small unlit marker cubes mark each light's own position/
// color so the layout is readable without needing to know the coordinates.
class LightPriorityExample : public BaseExample {

public:

	LightPriorityExample();
	virtual ~LightPriorityExample();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	// Point Lights
	std::vector<GameObject*> lightObjects;
	std::vector<PointLight*> pointLights;
	std::vector<Vec4> lightColors;

	// Unlit marker cubes, one per light, colored to match, so the light
	// layout is visible even where no test cube is lit.
	Renderable* markerMesh;
	std::vector<GenericShaderMaterial*> markerMaterials;
	std::vector<RenderingComponent*> markerComponents;

	// Test Cubes - lit, shared shape/material, only position differs
	Renderable* cubeMesh;
	GenericShaderMaterial* cubeMaterial;
	std::vector<GameObject*> cubeObjects;
	std::vector<RenderingComponent*> cubeComponents;

};

#endif	/* LIGHTPRIORITYEXAMPLE_H */
