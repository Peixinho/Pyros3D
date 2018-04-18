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

#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Core/Octree/Octree.h>
using namespace p3d;

class LOD_Example : public ClassName
{

public:

	LOD_Example();
	virtual ~LOD_Example();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	// Scene
	SceneGraph* Scene;
	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// Camera - Its a regular GameObject
	GameObject* Camera;
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
	void OnMouseRelease(Event::Input::Info e);
	void AddTeapot(Event::Input::Info e);

	float counterX, counterY;
	Vec2 mouseCenter, mouseLastPosition, mousePosition;
	bool _moveFront, _moveBack, _strafeLeft, _strafeRight;

	Octree* octree;
	bool generatedOctree;
};

#endif	/* LOD_EXAMPLE_H */
