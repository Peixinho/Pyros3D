//============================================================================
// Name        : PickingPainterMethod.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : PickingExample With Painter Method
//============================================================================

#ifndef PICKINGPAINTERMETHOD_H
#define	PICKINGPAINTERMETHOD_H

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Utils/Mouse3D/PainterPick.h>

using namespace p3d;

class PickingPainterMethod : public BaseExample {

public:

	PickingPainterMethod();
	virtual ~PickingPainterMethod();

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
	Renderable* cubeHandle;

	// Light
	GameObject* Light;
	DirectionalLight* dLight;

	// Event On Mouse Release
	void OnMouseRelease(Event::Input::Info e);

	// Painter Method
	PainterPick* picking;

	// Material for Selected Mesh
	GenericShaderMaterial *UnselectedMaterial, *SelectedMaterial;

	// Selected Mesh
	RenderingMesh* SelectedMesh;
};

#endif	/* PICKINGPAINTERMETHOD_H */
