//============================================================================
// Name        : CustomMaterial.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Rotating Cube Example
//============================================================================

#ifndef CustomMaterial_H
#define	CustomMaterial_H

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Materials/CustomShaderMaterials/CustomShaderMaterial.h>

using namespace p3d;

class CustomMaterialExample : public CustomShaderMaterial {

public:

    CustomMaterialExample() : CustomShaderMaterial(STR(EXAMPLES_PATH)"/assets/custommaterialshader.glsl")
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

class CustomMaterial : public BaseExample {

public:

	CustomMaterial();
	virtual ~CustomMaterial();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);

private:

	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
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

