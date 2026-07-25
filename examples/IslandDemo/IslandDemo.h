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

		// See IMaterial.h's comment on extraUniforms[2] - matches
		// WaterShader.glsl's WaterVertParams/WaterFragParams blocks
		// exactly. uColor registered above is never actually declared in
		// that shader (a pre-existing, harmless dead registration - GL's
		// glGetUniformLocation() already silently no-ops it), so it has
		// no entry here either.
		extraUniforms[0].binding = 40;
		extraUniforms[0].blockName = "WaterVertParams";
		extraUniforms[0].size = 204;
		extraUniforms[0].scratch.resize(extraUniforms[0].size, 0);
		extraUniforms[0].offsets["uProjectionMatrix"] = 0;
		extraUniforms[0].offsets["uViewMatrix"] = 64;
		extraUniforms[0].offsets["uModelMatrix"] = 128;
		extraUniforms[0].offsets["uCameraPos"] = 192;

		extraUniforms[1].binding = 41;
		extraUniforms[1].blockName = "WaterFragParams";
		extraUniforms[1].size = 12;
		extraUniforms[1].scratch.resize(extraUniforms[1].size, 0);
		extraUniforms[1].offsets["uNearFarPlane"] = 0;
		extraUniforms[1].offsets["uTime"] = 8;
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
	virtual void DrawUI();

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

