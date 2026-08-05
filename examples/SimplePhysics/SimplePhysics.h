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
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Physics/Physics.h>
#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <memory>

using namespace p3d;

class SimplePhysics : public BaseExample {

public:

	SimplePhysics();
	virtual ~SimplePhysics();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// GameObject
	std::vector<std::shared_ptr<GameObject>> Cubes;
	std::vector<std::shared_ptr<RenderingComponent>> rCubes;
	std::vector<std::shared_ptr<IPhysicsComponent>> pCubes;
	// Light
	std::shared_ptr<GameObject> Light;
	std::shared_ptr<DirectionalLight> dLight;

	// Floor GameObject
	std::shared_ptr<GameObject> Floor;
	// Floor Rendering Component
	std::shared_ptr<RenderingComponent> rFloor;
	// Floor Physics Component
	std::shared_ptr<IPhysicsComponent> pFloor;
	// Mesh - one Cube shared by 1000 RenderingComponents (Stage 2 proof)
	std::shared_ptr<Renderable> cubeHandle, floorHandle;

	// Physics Method
	Physics* physics;

	// Material for Selected Mesh
	std::shared_ptr<GenericShaderMaterial> Diffuse;

	// Selected Mesh
	RenderingMesh* SelectedMesh;
};

#endif	/* SIMPLEPHYSICS_H */
