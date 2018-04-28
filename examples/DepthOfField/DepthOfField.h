//============================================================================
// Name        : DepthOfField.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef DEPTHOFFIELD_H
#define	DEPTHOFFIELD_H

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

#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurXEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurYEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/RTTDebug.h>

using namespace p3d;

class DepthOfField : public ClassName
{

public:

	DepthOfField();
	virtual ~DepthOfField();

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
	GameObject* Light;
	DirectionalLight* dLight;

	std::vector<GameObject*> go;
	std::vector<RenderingComponent*> rc;

	// Mesh
	Renderable* modelMesh;

	// Effect manager
	PostEffectsManager* EffectManager;

	// Effects
	IEffect* blurX, *blurY, *resize, *blurXlow, *blurYlow, *depthOfField;

	Texture* fullResBlur;
	Texture* lowResBlur;

};

class DepthOfFieldEffect : public IEffect {
public:
	DepthOfFieldEffect(Texture* texture1, Texture* texture2, const uint32 Width, const uint32 Height);
	virtual ~DepthOfFieldEffect() {}
};

#endif	/* DEPTHOFFIELD_H */

