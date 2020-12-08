//============================================================================
// Name        : DeferredRendering.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef DEFERREDRENDERING_H
#define DEFERREDRENDERING_H

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>

using namespace p3d;

class DeferredRendering : public BaseExample
{

public:

	DeferredRendering();
	virtual ~DeferredRendering();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	// Renderer
	DeferredRenderer* Renderer;
	// Projection
	Projection projection;
	// Light
	std::vector<GameObject*> Lights;
	std::vector<PointLight*> pLights;

	Renderable* cubeHandle;
	std::vector<GameObject*> Cubes;
	std::vector<RenderingComponent*> rCubes;

	// Material
	GenericShaderMaterial* Diffuse;

	// Deferred Settings
	Texture* albedoTexture, *specularTexture, *depthTexture, *normalTexture;
	FrameBuffer* deferredFBO;

};

#endif  /* DEFERREDRENDERING_H */

