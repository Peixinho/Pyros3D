//============================================================================
// Name        : RotatingCube.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef ROTATINGCUBE_H
#define ROTATINGCUBE_H

#include "../BaseExample//BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/AnimationManager/TextureAnimation.h>

using namespace p3d;


class RotatingTextureAnimatedCube : public BaseExample {
public:

	RotatingTextureAnimatedCube();
	virtual ~RotatingTextureAnimatedCube();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

	void OnMousePress(Event::Input::Info e);

private:

	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// GameObject
	GameObject* CubeObject;
	// Rendering Component
	RenderingComponent* rCube;
	// Mesh
	Renderable* cubeMesh;
	// Material
	GenericShaderMaterial* material;
	// Animation
	TextureAnimation* anim;
	// Animation Instance
	TextureAnimationInstance* animInst;
	// Textures
	Texture* tex0; Texture* tex1; Texture* tex2; Texture* tex3; Texture* tex4; Texture* tex5;
};

#endif  /* ROTATINGTEXTUREANINATEDCUBE_H */
