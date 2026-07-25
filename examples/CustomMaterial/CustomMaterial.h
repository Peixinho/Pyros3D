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

		// See IMaterial.h's comment on extraUniforms[2] - matches
		// custommaterialshader.glsl's CustomMaterialVertParams/
		// CustomMaterialFragParams blocks exactly.
		extraUniforms[0].binding = 35;
		extraUniforms[0].blockName = "CustomMaterialVertParams";
		extraUniforms[0].size = 192;
		extraUniforms[0].scratch.resize(extraUniforms[0].size, 0);
		extraUniforms[0].offsets["uProjectionMatrix"] = 0;
		extraUniforms[0].offsets["uViewMatrix"] = 64;
		extraUniforms[0].offsets["uModelMatrix"] = 128;

		extraUniforms[1].binding = 36;
		extraUniforms[1].blockName = "CustomMaterialFragParams";
		extraUniforms[1].size = 16;
		extraUniforms[1].scratch.resize(extraUniforms[1].size, 0);
		extraUniforms[1].offsets["uColor"] = 0;
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
	virtual void DrawUI();

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

