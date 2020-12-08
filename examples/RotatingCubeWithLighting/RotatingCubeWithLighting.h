//============================================================================
// Name        : RotatingCubeWithLighting.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube With Light Example
//============================================================================

#ifndef ROTATINGCUBEWITHLIGHT_H
#define	ROTATINGCUBEWITHLIGHT_H

#include "../BaseExample/BaseExample.h"

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>

using namespace p3d;

class RotatingCubeWithLighting : public BaseExample {

public:

	RotatingCubeWithLighting();
	virtual ~RotatingCubeWithLighting();

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
	// GameObject
	GameObject* CubeObject;
	// Rendering Component
	RenderingComponent* rCube;
	// Mesh
	Renderable* cubeMesh;
	// Material
	GenericShaderMaterial* Diffuse;

};

#endif	/* ROTATINGCUBEWITHLIGHT_H */

