//============================================================================
// Name        : LightPriorityExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "LightPriorityExample.h"

using namespace p3d;

LightPriorityExample::LightPriorityExample() : BaseExample(1024, 768, "Pyros3D - Light Priority Example", WindowType::Close | WindowType::Resize)
{

}

void LightPriorityExample::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Perspective(70.f, (f32)width / (f32)height, 1.f, 3000.f);
}

void LightPriorityExample::Init()
{
	// Initialization

	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// No ambient - unlit areas should read as black, so it's obvious which
	// lights are/aren't reaching a given cube.
	Renderer->SetGlobalLight(Vec4(0, 0, 0, 1));

	// Projection
	projection.Perspective(70.f, (f32)Width / (f32)Height, 1.f, 3000.f);

	FPSCamera->SetPosition(Vec3(0, 80, 900));

	// Six point lights on a line, 200 units apart, each a distinct color.
	// Radius 1000 keeps every light within range of every cube below, so
	// which 4 actually light a cube is decided purely by the nearest-4
	// sort, not by attenuation cutoff.
	lightColors.push_back(Vec4(1, 0, 0, 1));    // red
	lightColors.push_back(Vec4(1, 0.5, 0, 1));  // orange
	lightColors.push_back(Vec4(1, 1, 0, 1));    // yellow
	lightColors.push_back(Vec4(0, 1, 0, 1));    // green
	lightColors.push_back(Vec4(0, 0, 1, 1));    // blue
	lightColors.push_back(Vec4(1, 0, 1, 1));    // magenta

	markerMesh = new Cube(20, 20, 20);

	for (size_t i = 0; i < lightColors.size(); i++)
	{
		f32 x = -500.f + (f32)i * 200.f;

		GameObject* lightObject = new GameObject();
		lightObject->SetPosition(Vec3(x, 80, 0));

		PointLight* pLight = new PointLight(lightColors[i], 1000.f);
		lightObject->Add(pLight);

		// Unlit marker so the light's position/color reads clearly even
		// where it isn't the dominant light on any cube.
		GenericShaderMaterial* markerMat = new GenericShaderMaterial(ShaderUsage::Color);
		markerMat->SetColor(lightColors[i]);
		RenderingComponent* markerComp = new RenderingComponent(markerMesh, markerMat);
		lightObject->Add(markerComp);

		Scene->Add(lightObject);

		lightObjects.push_back(lightObject);
		pointLights.push_back(pLight);
		markerMaterials.push_back(markerMat);
		markerComponents.push_back(markerComp);
	}

	// Three test cubes, positioned between the lights so each one has a
	// clear, tie-free nearest-4 ranking:
	//   x=-400 -> nearest 4: red, orange, yellow, green   (drops blue, magenta)
	//   x=0    -> nearest 4: yellow, green, orange, blue  (drops red, magenta)
	//   x=400  -> nearest 4: blue, magenta, green, yellow (drops red, orange)
	// White material so the lit color is as close as possible to the raw
	// light colors, making the dropped/kept lights easy to eyeball.
	cubeMesh = new Cube(60, 60, 60);
	cubeMaterial = new GenericShaderMaterial(ShaderUsage::Color | ShaderUsage::Diffuse);
	cubeMaterial->SetColor(Vec4(1, 1, 1, 1));

	f32 cubeX[3] = { -400.f, 0.f, 400.f };
	for (size_t i = 0; i < 3; i++)
	{
		GameObject* cubeObject = new GameObject();
		cubeObject->SetPosition(Vec3(cubeX[i], 80, 0));

		RenderingComponent* cubeComp = new RenderingComponent(cubeMesh, cubeMaterial);
		cubeObject->Add(cubeComp);

		Scene->Add(cubeObject);

		cubeObjects.push_back(cubeObject);
		cubeComponents.push_back(cubeComp);
	}

	// Initialize ImGui
	InitImGui();
}

void LightPriorityExample::Update()
{
	// Update - Game Loop

	Scene->Update(GetTime());

	BaseExample::Update();

	// Render Scene
	Renderer->PreRender(FPSCamera, Scene);
	Renderer->RenderScene(projection, FPSCamera, Scene);

	// Render ImGui
	RenderImGui();
}

void LightPriorityExample::DrawUI()
{
	// Draw base UI (FPS, etc.)
	DrawBaseUI();

	if (ImGui::Begin("Light Priority Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Nearest-Light Selection Test");
		ImGui::Separator();

		ImGui::Text("6 point lights on a line, 200 units apart:");
		ImGui::Text("  red, orange, yellow, green, blue, magenta");
		ImGui::Text("Each light's radius covers the whole row, so all 6");
		ImGui::Text("are always 'in range' - only the nearest-4 sort");
		ImGui::Text("decides which ones actually light a given cube.");

		ImGui::Separator();

		ImGui::Text("3 white cubes, expected dominant colors:");
		ImGui::Text("  left cube   (x=-400): red/orange/yellow/green");
		ImGui::Text("  center cube (x=0):    orange/yellow/green/blue");
		ImGui::Text("  right cube  (x=400):  yellow/green/blue/magenta");
		ImGui::Text("Small unlit cubes mark each light's own position/color.");

		ImGui::Separator();

		ImGui::Text("Performance:");
		ImGui::Text("  FPS: %.1f", (float)fps.getFPS());
		ImGui::Text("  Resolution: %dx%d", Width, Height);
	}
	ImGui::End();
}

void LightPriorityExample::Shutdown()
{
	// All your Shutdown Code Here

	for (size_t i = 0; i < cubeObjects.size(); i++)
	{
		Scene->Remove(cubeObjects[i]);
		cubeObjects[i]->Remove(cubeComponents[i]);
		delete cubeComponents[i];
		delete cubeObjects[i];
	}
	delete cubeMaterial;
	delete cubeMesh;

	for (size_t i = 0; i < lightObjects.size(); i++)
	{
		Scene->Remove(lightObjects[i]);
		lightObjects[i]->Remove(pointLights[i]);
		lightObjects[i]->Remove(markerComponents[i]);
		delete markerComponents[i];
		delete markerMaterials[i];
		delete pointLights[i];
		delete lightObjects[i];
	}
	delete markerMesh;

	// Renderer is deleted by BaseExample::Shutdown()
	BaseExample::Shutdown();
}

LightPriorityExample::~LightPriorityExample() {}
