//============================================================================
// Name        : RotatingCube.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef ROTATINGCUBE_H
#define ROTATINGCUBE_H

#define _STR(path) #path
#define STR(path) _STR(path)

#if defined(_SDL)
#include "../WindowManagers/SDL/SDLContext.h"
#define ClassName SDLContext
#elif defined(_SDL2)
#include "../WindowManagers/SDL2/SDL2Context.h"
#define ClassName SDL2Context
#else
#include "../WindowManagers/SFML/SFMLContext.h"
#define ClassName SFMLContext
#endif

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/AnimationManager/TextureAnimation.h>

using namespace p3d;


class RotatingTextureAnimatedCube : public ClassName {
public:

	RotatingTextureAnimatedCube();
	virtual ~RotatingTextureAnimatedCube();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

	void OnMousePress(Event::Input::Info e);

private:

	// Scene
	SceneGraph* Scene;
	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// Camera - Its a regular GameObject
	GameObject* Camera;
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
