//============================================================================
// Name        : Decals.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef DECALSEXAMPLE_H
#define	DECALSEXAMPLE_H

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Assets/Renderable/Decals/Decals.h>
#include <Pyros3D/Utils/Mouse3D/Mouse3D.h>

using namespace p3d;

class Decals : public BaseExample
{

public:

	Decals();
	virtual ~Decals();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

	void OnMouseRelease(Event::Input::Info e);
private:

	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// GameObject
	GameObject* CubeObject, *SphereObject, *ModelObject;
	// Rendering Component
	RenderingComponent* rCube, *rSphere, *rModel;
	// Mesh
	Renderable* cubeMesh, *sphereMesh, *modelMesh;

	GenericShaderMaterial* decalMaterial;
	bool GetIntersectedTriangle(RenderingComponent* rcomp, Mouse3D mouse, Vec3* intersection, Vec3* normal, uint32* meshID);
	void CreateDecal();
	std::vector<DecalGeometry*> decals;
	std::vector<RenderingComponent*> rdecals;

};

#endif	/* DECALSEXAMPLE */

