//============================================================================
// Name        : CppApiDemo.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Minimal traditional C++ API sample - ForwardRenderer, one
//                rotating cube, one directional light, free-fly camera via
//                BaseExample. Scene JSON / Lua demos live in DemoLauncher.
//============================================================================

#ifndef CPPAPIDEMO_H
#define	CPPAPIDEMO_H

#include "../BaseExample/BaseExample.h"

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <memory>

using namespace p3d;

class CppApiDemo : public BaseExample {

public:

	CppApiDemo();
	virtual ~CppApiDemo();

	virtual void Init();
	virtual void Update();
	virtual void Shutdown();
	virtual void OnResize(const uint32 width, const uint32 height);
	virtual void DrawUI();

private:

	Projection projection;

	std::shared_ptr<GameObject> cubeObj;
	std::shared_ptr<RenderingComponent> rCube;
	std::shared_ptr<Renderable> cubeMesh;
	std::shared_ptr<GenericShaderMaterial> material;

	std::shared_ptr<GameObject> lightObj;
	std::shared_ptr<DirectionalLight> dirLight;
};

#endif	/* CPPAPIDEMO_H */
