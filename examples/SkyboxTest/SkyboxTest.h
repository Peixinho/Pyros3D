//============================================================================
// Name        : SkyboxTest.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Real cubemap texture upload test (skybox)
//============================================================================

#ifndef SKYBOXTEST_H
#define	SKYBOXTEST_H

#include "../BaseExample/BaseExample.h"
#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>

using namespace p3d;

class SkyboxTest : public BaseExample {
public:

	SkyboxTest();
	virtual ~SkyboxTest();
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
	GameObject* SkyboxObject;
	// Rendering Component
	RenderingComponent* rSkybox;
	// Mesh
	Renderable* skyboxMesh;
	// Material
	GenericShaderMaterial* material;
	// Real cubemap texture, loaded from 6 separate face images -
	// exercises VulkanRenderDevice::UploadTexture2D()'s isCubemapTarget
	// path with real color data (as opposed to CreateEmptyTexture(),
	// which every point-light shadow cubemap already exercises).
	Texture* skyboxTexture;

};

#endif	/* SKYBOXTEST_H */
