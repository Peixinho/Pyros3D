//============================================================================
// Name        : SimplePhysics.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Simple Physics Example
//============================================================================

#ifndef SIMPLEPHYSICS_H
#define	SIMPLEPHYSICS_H

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Physics/Physics.h>
#include <Pyros3D/Physics/Components/IPhysicsComponent.h>

using namespace p3d;

class SimplePhysics : public BaseExample {

public:

	SimplePhysics();
	virtual ~SimplePhysics();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// GameObject
	std::vector<GameObject*> Cubes;
	std::vector<RenderingComponent*> rCubes;
	std::vector<IPhysicsComponent*> pCubes;
	Renderable* cubeMesh;
	// Light
	GameObject* Light;
	DirectionalLight* dLight;

	// Floor GameObject
	GameObject* Floor;
	// Floor Rendering Component
	RenderingComponent* rFloor;
	// Floor Physics Component
	IPhysicsComponent* pFloor;
	// Mesh
	Renderable* cubeHandle, *floorHandle;

	// Physics Method
	Physics* physics;

	// Material for Selected Mesh
	GenericShaderMaterial *Diffuse;

	// Selected Mesh
	RenderingMesh* SelectedMesh;
};

#endif	/* SIMPLEPHYSICS_H */
