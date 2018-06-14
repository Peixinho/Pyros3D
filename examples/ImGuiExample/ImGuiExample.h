//============================================================================
// Name        : ImGuiExample.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : ImGui Example
//============================================================================

#ifndef IMGUIEXAMPLE_H
#define	IMGUIEXAMPLE_H

#include "imgui_sdl2_context.h"
#define ClassName p3d::ImGuiContext

#include <Pyros3D/Assets/Renderable/Primitives/Shapes/Cube.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Utils/Colors/Colors.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Rendering/Components/Lights/DirectionalLight/DirectionalLight.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>

using namespace p3d;

class ImGuiExample : public ClassName
{

public:

	ImGuiExample();
	virtual ~ImGuiExample();

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

	ImVec4 clear_color;
	bool show_test_window;
	bool show_another_window;
};

#endif	/* IMGUIEXAMPLE_H */
