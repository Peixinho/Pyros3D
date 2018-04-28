//============================================================================
// Name        : CustomMaterial.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef CustomMaterial_H
#define	CustomMaterial_H

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
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>

using namespace p3d;

class CustomMaterialExample : public CustomShaderMaterial {

public:

	CustomMaterialExample() : CustomShaderMaterial(STR(EXAMPLES_PATH)"/Pyros3D/CustomMaterial/assets/shader.glsl")
	{
		AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		handle = AddUniform(Uniform("uColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
	}

	virtual void PreRender()
	{
		srand((unsigned int)time(NULL));
		Vec4 color = Vec4((rand() % 100) / 100.f, (rand() % 100) / 100.f, (rand() % 100) / 100.f, 1.f);
		handle->SetValue(&color);
	}
	virtual void Render() {}
	virtual void AfterRender() {}

	Uniform* handle;

};

class CustomMaterial : public ClassName {

public:

	CustomMaterial();
	virtual ~CustomMaterial();

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
	CustomMaterialExample* Material;

};

#endif	/* CustomMaterial_H */

