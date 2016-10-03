//============================================================================
// Name        : IslandDemo.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef IslandDemo_H
#define	IslandDemo_H

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

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Plane.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
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

class IslandDemo : public ClassName
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
	SceneGraph* Scene;
	SceneGraph* SceneWater;
	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// Camera - Its a regular GameObject
	GameObject* Camera;
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

	float counterX, counterY;
	Vec2 mouseCenter, mouseLastPosition, mousePosition;
	bool _moveFront, _moveBack, _strafeLeft, _strafeRight;

};

#endif	/* IslandDemo_H */

