//============================================================================
// Name        : DepthOfField.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef DEPTHOFFIELD_H
#define	DEPTHOFFIELD_H

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurXEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/BlurYEffect.h>
#include <Pyros3D/Rendering/PostEffects/Effects/RTTDebug.h>

using namespace p3d;

class DepthOfField : public BaseExample
{

public:

	DepthOfField();
	virtual ~DepthOfField();

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

