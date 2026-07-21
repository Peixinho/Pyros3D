//============================================================================
// Name        : ParallaxMapping.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Parallax Mapping Example
//============================================================================

#ifndef ParallaxMapping_H
#define	ParallaxMapping_H

#define _STR(path) #path
#define STR(path) _STR(path)

// Context selection (ClassName + the right window-manager header) is
// handled once, correctly, by BaseExample.h - this file used to carry its
// own duplicate copy of that logic that never learned about
// _SDL2VULKAN and silently fell through to SFML for any context it didn't
// recognize (i.e. every Vulkan build), forcing a hard SFML dependency this
// example never otherwise needed.
#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>

using namespace p3d;

class ParallaxMapping : public BaseExample {

public:

	ParallaxMapping();
	virtual ~ParallaxMapping();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

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

	GameObject* Light;
	DirectionalLight* dLight;

	Texture *texturemap, *normalmap, *displacementmap;

	float counterX, counterY;
	Vec2 mouseCenter, mouseLastPosition, mousePosition;
	bool _moveFront, _moveBack, _strafeLeft, _strafeRight;

	// Events
	void MoveFrontPress(Event::Input::Info e);
	void MoveBackPress(Event::Input::Info e);
	void StrafeLeftPress(Event::Input::Info e);
	void StrafeRightPress(Event::Input::Info e);
	void MoveFrontRelease(Event::Input::Info e);
	void MoveBackRelease(Event::Input::Info e);
	void StrafeLeftRelease(Event::Input::Info e);
	void StrafeRightRelease(Event::Input::Info e);
	void LookTo(Event::Input::Info e);

};

#endif	/* ParallaxMapping_H */

