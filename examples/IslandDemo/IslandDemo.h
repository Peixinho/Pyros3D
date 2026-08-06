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
#include <memory>

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
		// std140: 3x mat4 (192) + vec3 at 192 padded to 16 → 208
		extraUniforms[0].size = 208;
		extraUniforms[0].scratch.resize(extraUniforms[0].size, 0);
		extraUniforms[0].offsets["uProjectionMatrix"] = 0;
		extraUniforms[0].offsets["uViewMatrix"] = 64;
		extraUniforms[0].offsets["uModelMatrix"] = 128;
		extraUniforms[0].offsets["uCameraPos"] = 192;

		extraUniforms[1].binding = 41;
		extraUniforms[1].blockName = "WaterFragParams";
		// std140: vec2 at 0 + float at 8, block rounded to 16
		extraUniforms[1].size = 16;
		extraUniforms[1].scratch.resize(extraUniforms[1].size, 0);
		extraUniforms[1].offsets["uNearFarPlane"] = 0;
		extraUniforms[1].offsets["uTime"] = 8;
	}

	virtual ~WaterMaterial() {}
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

	// Renderer
	ForwardRenderer* Renderer;
	// Projection
	Projection projection;
	// Camera - Its a regular GameObject
	std::shared_ptr<GameObject> CameraReflection;
	// GameObject
	std::shared_ptr<GameObject> gIsland;
	// Rendering Component
	std::shared_ptr<RenderingComponent> rIsland;
	// Mesh
	std::shared_ptr<Renderable> island;

	std::shared_ptr<GameObject> Light;
	std::shared_ptr<DirectionalLight> dLight;

	// Water (same Scene as the island - one final swapchain RenderScene.
	// A second SceneWater pass used to call BeginFrame/EndFrame twice per
	// frame on Vulkan and flash.)
	std::shared_ptr<GameObject> gWater;
	std::shared_ptr<Renderable> water;
	std::shared_ptr<RenderingComponent> rWater;
	std::shared_ptr<WaterMaterial> matWater;

	std::shared_ptr<Texture> normalMap, DUDVmap;

	FrameBuffer* fboReflection;
	std::shared_ptr<Texture> reflectionTexture;
	FrameBuffer* fboRefraction;
	std::shared_ptr<Texture> refractionTexture;
	std::shared_ptr<Texture> refractionTextureDepth;

	void SetIslandCullFace(const uint32 face);
};

#endif	/* IslandDemo_H */
