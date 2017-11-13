//============================================================================
// Name        : ParallaxMapping.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Parallax Mapping Example
//============================================================================

#ifndef ParallaxMapping_H
#define	ParallaxMapping_H

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
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>

using namespace p3d;

class ParallaxMappingExample : public CustomShaderMaterial {

public:

	ParallaxMappingExample() : CustomShaderMaterial("../examples/ParallaxMapping/assets/shader.glsl")
	{
		AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		AddUniform(Uniform("uViewPos", Uniforms::DataUsage::CameraPosition));

		handleTexture = AddUniform(Uniform("uTexturemap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		handleNormal = AddUniform(Uniform("uNormalmap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		handleDispl = AddUniform(Uniform("uDisplacementmap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		handleScale = AddUniform(Uniform("uHeightScale", Uniforms::DataUsage::Other, Uniforms::DataType::Float));

		int a = 0;
		handleTexture->SetValue(&a);a++;
		handleNormal->SetValue(&a);a++;
		handleDispl->SetValue(&a);

		texturemap = new Texture();
		normalmap = new Texture();
		displacementmap = new Texture();

		texturemap->LoadTexture("../examples/ParallaxMapping/assets/bricks.jpg");
		normalmap->LoadTexture("../examples/ParallaxMapping/assets/bricks_normal.jpg");
		displacementmap->LoadTexture("../examples/ParallaxMapping/assets/bricks_disp.jpg");
	}

	virtual void PreRender()
	{
		texturemap->Bind();
		normalmap->Bind();
		displacementmap->Bind();
		f32 v = 0.05f;
		handleScale->SetValue(&v);
	}
	virtual void Render() {}
	virtual void AfterRender() 
	{
		displacementmap->Unbind();
		normalmap->Unbind();
		texturemap->Unbind();
	}

	Uniform* handle, *handleTexture, *handleNormal, *handleDispl, *handleScale;
	Texture *texturemap, *normalmap, *displacementmap;

	virtual ~ParallaxMappingExample()
	{
		delete texturemap;
		delete normalmap;
		delete displacementmap;
	}

};

class ParallaxMapping : public ClassName {

public:

	ParallaxMapping();
	virtual ~ParallaxMapping();

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
	// GameObject
	GameObject* CubeObject;
	// Rendering Component
	RenderingComponent* rCube;
	// Mesh
	Renderable* cubeMesh;
	// Custom Material
	ParallaxMappingExample* Material;

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

