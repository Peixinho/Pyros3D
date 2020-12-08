//============================================================================
// Name        : IslandDemo.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef IslandDemo_H
#define	IslandDemo_H

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>

using namespace p3d;

class WaterMaterial : public CustomShaderMaterial {
public:
	WaterMaterial(const std::string &shader) : CustomShaderMaterial(shader)
	{
		AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		AddUniform(Uniform("uColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		AddUniform(Uniform("uTime", Uniforms::DataUsage::Timer));
		AddUniform(Uniform("uCameraPos", Uniforms::DataUsage::CameraPosition));
		AddUniform(Uniform("uNearFarPlane", Uniforms::DataUsage::NearFarPlane));
	}

	virtual ~WaterMaterial() {
		for (std::vector<Texture*>::iterator i = textures.begin(); i != textures.end(); i++)
			delete (*i);
	}

	virtual void PreRender()
	{
		for (std::vector<Texture*>::iterator i = textures.begin(); i != textures.end(); i++)
			(*i)->Bind();
	}

	virtual void AfterRender()
	{
		for (std::vector<Texture*>::reverse_iterator i = textures.rbegin(); i != textures.rend(); i++)
			(*i)->Unbind();
	}

	std::vector<Texture*> textures;
};

class IslandDemo : public BaseExample
{

public:

	IslandDemo();
	virtual ~IslandDemo();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	// Scene
	SceneGraph* SceneWater;
	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// Camera - Its a regular GameObject
	GameObject* CameraReflection;
	// GameObject
	GameObject* gIsland;
	// Rendering Component
	RenderingComponent* rIsland;
	// Mesh
	Renderable* island;

	GameObject* Light;
	DirectionalLight* dLight;

	// Water
	GameObject* gWater;
	Renderable* water;
	RenderingComponent* rWater;
	WaterMaterial* matWater;

	Texture *normalMap, *DUDVmap;

	FrameBuffer* fboReflection;
	Texture* reflectionTexture;
	FrameBuffer* fboRefraction;
	Texture* refractionTexture;
	Texture* refractionTextureDepth;
};

#endif	/* IslandDemo_H */

