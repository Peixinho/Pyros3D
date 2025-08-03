//============================================================================
// Name        : ScreenSpaceReflection.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Screen Space Reflection Example
//============================================================================

#ifndef SCREENSPACEREFLECTION_H
#define	SCREENSPACEREFLECTION_H

#define _STR(path) #path
#define STR(path) _STR(path)

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Sphere.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cylinder.h>
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Torus.h>
#include <Pyros3D/Assets/Renderable/Models/Model.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>

#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Utils/Mouse3D/PainterPick.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/PostEffects/Effects/ScreenSpaceReflectionEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/SSAOEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurSSAOEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.h>

using namespace p3d;

class SSRFinalEffect : public IEffect {
public:
	SSRFinalEffect(uint32 texture1, uint32 texture2, const uint32 Width, const uint32 Height);
	virtual ~SSRFinalEffect() {}
};

class ScreenSpaceReflection : public BaseExample {

public:

	ScreenSpaceReflection();
	virtual ~ScreenSpaceReflection();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;

	// Effect manager
	PostEffectsManager* EffectManager;

	ScreenSpaceReflectionEffect* ssr;
	SSRFinalEffect* ssrFinal;
	// Light
	GameObject* Light;
	DirectionalLight* dLight;

	Renderable* teapot;
	std::vector<GameObject*> gTeapots;
	std::vector<RenderingComponent*> rTeapots;

	Renderable* floor;
	GameObject* gFloor;
	RenderingComponent* rFloor;

	// ImGui Controls for SSR
	float ssrMaxSteps;
	float ssrMaxDistance;
	float ssrThickness;
	float ssrReflectionStrength;
	float ssrFloorHeight;
	float ssrFloorTolerance;
	bool ssrDebugMode;
};

#endif	/* SCREENSPACEREFLECTION_H */

