//============================================================================
// Name        : MotionBlurExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef MOTIONBLUREXAMPLE_H
#define	MOTIONBLUREXAMPLE_H

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Renderer/SpecialRenderers/VelocityRenderer/VelocityRenderer.h>
#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/PostEffects/Effects/MotionBlurEffect.h>
using namespace p3d;

#define _STR(path) #path
#define STR(path) _STR(path)

class MotionBlurExample : public BaseExample
{

public:

	MotionBlurExample();
	virtual ~MotionBlurExample();

	void Init();
	void Update();
	void Shutdown();
	void OnResize(const uint32 width, const uint32 height);

private:

	// Renderer
	ForwardRenderer* Renderer;
	VelocityRenderer* VRenderer;
	// Effect manager
	PostEffectsManager* EffectManager;
	// Effect
	IEffect* MotionBlur;
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
	// Texture
	Texture* texture;
};

#endif	/* MOTIONBLUREXAMPLE_H */

