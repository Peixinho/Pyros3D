//============================================================================
// Name        : SSAOExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : SSAO Example
//============================================================================

#ifndef SSAOEXAMPLE_H
#define	SSAOEXAMPLE_H

#define _STR(path) #path
#define STR(path) _STR(path)

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Utils/Mouse3D/PainterPick.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/PostEffects/Effects/SSAOEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurSSAOEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.h>

using namespace p3d;

class SSAOExample : public BaseExample {

public:

	SSAOExample();
	virtual ~SSAOExample();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// Camera - Its a regular GameObject
	GameObject* Camera;

	// Effect manager
	PostEffectsManager* EffectManager;

	SSAOEffect* ssao;
	// Light
	GameObject* Light;
	DirectionalLight* dLight;

	Renderable* teapot;
	std::vector<GameObject*> gTeapots;
	std::vector<RenderingComponent*> rTeapots;

	Renderable* floor;
	GameObject* gFloor;
	RenderingComponent* rFloor;

};

class SSAOEffectFinal : public IEffect {
public:
	SSAOEffectFinal(uint32 texture1, uint32 texture2, const uint32 Width, const uint32 Height);
	virtual ~SSAOEffectFinal() {}
};

#endif	/* SSAOEXAMPLE_H */
