//============================================================================
// Name        : RotatingCubeWithLightingAndShadow.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube With Light And Shadow Example
//============================================================================

#ifndef ROTATINGCUBEWITHLIGHTANDSHADOW_H
#define	ROTATINGCUBEWITHLIGHTANDSHADOW_H

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>

using namespace p3d;

class RotatingCubeWithLightingAndShadow : public BaseExample {
public:

	RotatingCubeWithLightingAndShadow();
	virtual ~RotatingCubeWithLightingAndShadow();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// Light
	GameObject* Light;
	DirectionalLight* dLight;

	GameObject* Light2;
	PointLight* pLight;
	//SpotLight* pLight;

	// Cube GameObject
	GameObject* CubeObject;
	// Rendering Component
	RenderingComponent* rCube;
	// Mesh
	Renderable* cubeMesh, *floorMesh;
	// Floor GameObject
	GameObject* Floor, *Ceiling;
	// Floor Rendering Component
	RenderingComponent* rFloor, *rCeiling;
	// Floor Material
	GenericShaderMaterial* FloorMaterial;
	// Material
	GenericShaderMaterial* Diffuse;
};

#endif	/* ROTATINGCUBEWITHLIGHTANDSHADOW_H */
