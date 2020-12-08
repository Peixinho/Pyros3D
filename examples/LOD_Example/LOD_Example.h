//============================================================================
// Name        : LOD_Example.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : LOD Example
//============================================================================

#ifndef LOD_EXAMPLE_H
#define	LOD_EXAMPLE_H

#define TEAPOTS 10000

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Core/Octree/Octree.h>
using namespace p3d;

class LOD_Example : public BaseExample
{

public:

	LOD_Example();
	virtual ~LOD_Example();

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
	std::vector<GameObject*> Lights;
	std::vector<PointLight*> pLights;

	// Objects List
	std::vector<GameObject*> Teapots;
	std::vector<RenderingComponent*> rTeapots;
	std::vector<GenericShaderMaterial*> mTeapots;
	Renderable* teapotLOD1Handle;
	Renderable* teapotLOD2Handle;
	Renderable* teapotLOD3Handle;

	Octree* octree;
	bool generatedOctree;
};

#endif	/* LOD_EXAMPLE_H */
