//============================================================================
// Name        : TextRendering.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Text Rendering Example
//============================================================================

#include "TextRendering.h"
#include <Pyros3D/Assets/Font/Font.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

using namespace p3d;

TextRendering::TextRendering() : BaseExample(1024, 768, "Pyros3D - Text Rendering", WindowType::Close | WindowType::Resize)
{

}

void TextRendering::OnResize(const uint32 width, const uint32 height)
{
	// Execute Parent Resize Function
	BaseExample::OnResize(width, height);

	// Resize
	Renderer->Resize(width, height);
	projection.Ortho(0.f, (f32)Width, 0.f, (f32)Height, 0.f, 10.f);
}

void TextRendering::Init()
{
	// Initialization
	BaseExample::Init();

	// Initialize Renderer
	Renderer = new ForwardRenderer(Width, Height);

	// Projection
	projection.Ortho(0.f, (f32)Width, 0.f, (f32)Height, 0.f, 10.f);

	// Create a simple camera for text rendering
	textCamera = new GameObject();
	textCamera->SetPosition(Vec3(0, 0, 1));
	Scene->Add(textCamera);

	// Create Font
	font = new Font(STR(EXAMPLES_PATH)"/assets/verdana.ttf", 16);
	font->CreateText("aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ,.0123456789[]()!?+-_\\|/ºª");

	// Create Text Material
	textMaterial = new GenericShaderMaterial(ShaderUsage::TextRendering);
	textMaterial->SetTextFont(font);

	// Create Game Object
	TextObject = new GameObject();
	textHandle = new Text(font, "Pyros3D Engine - Text Rendering\n\n:)", 16, 16, Vec4(1, 1, 1, 1));
	rText = new RenderingComponent(textHandle, textMaterial);
	TextObject->Add(rText);

	// Add GameObject to Scene
	Scene->Add(TextObject);
	Renderer->SetBackground(Vec4(0.2f, 0.2f, 0.2f, 0.2f));
	
	// Initialize ImGui
	InitImGui();
}

void TextRendering::Update()
{
	// Update - Game Loop
	BaseExample::Update();

	// Update Scene
	Scene->Update(GetTime());

	// Game Logic Here
	TextObject->SetRotation(Vec3(0, 0, (f32)GetTime()));
	TextObject->SetPosition(Vec3(Width*.5f, Height*.5f, 0.f));

	// Render Scene
	Renderer->PreRender(textCamera, Scene);
	Renderer->RenderScene(projection, textCamera, Scene);
	RenderImGui();
}

void TextRendering::DrawUI()
{
	// Text Rendering System Information
	if (ImGui::Begin("Text Rendering System Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Text Rendering System Information");
		ImGui::Separator();
		
		// Scene Information
		ImGui::Text("Scene Objects:");
		ImGui::Text("  Text Object: Rotating text in center");
		ImGui::Text("  Font: Verdana (16pt)");
		ImGui::Text("  Text Content: 'Pyros3D Engine - Text Rendering'");
		
		ImGui::Separator();
		
		// Text System Information
		ImGui::Text("Text Rendering System:");
		ImGui::Text("  Font File: verdana.ttf");
		ImGui::Text("  Font Size: 16 pixels");
		ImGui::Text("  Text Color: White (1, 1, 1, 1)");
		ImGui::Text("  Text Position: Center of screen");
		ImGui::Text("  Text Rotation: Z-axis rotation");
		
		ImGui::Separator();
		
		// Rendering Information
		ImGui::Text("Rendering System:");
		ImGui::Text("  Renderer: ForwardRenderer");
		ImGui::Text("  Projection: Orthographic");
		ImGui::Text("  Shader Usage: TextRendering");
		ImGui::Text("  Background: Gray (0.2, 0.2, 0.2, 0.2)");
		
		ImGui::Separator();
		
		// Performance Information
		ImGui::Text("Performance:");
		ImGui::Text("  Resolution: %dx%d", Width, Height);
		ImGui::Text("  Text Position: (%.1f, %.1f, %.1f)", 
			TextObject->GetPosition().x, 
			TextObject->GetPosition().y, 
			TextObject->GetPosition().z);
		
		ImGui::Separator();
		
		// Controls
		ImGui::Text("Controls:");
		ImGui::Text("  Text rotates automatically");
		ImGui::Text("  Text stays centered on screen");
	}
	ImGui::End();
}

void TextRendering::Shutdown()
{
	// All your Shutdown Code Here

	// Remove GameObjects From Scene
	Scene->Remove(TextObject);
	Scene->Remove(textCamera);

	TextObject->Remove(rText);

	// Delete
	delete rText;
	delete TextObject;
	delete textHandle;
	delete textMaterial;
	delete font;
	delete textCamera;

	// Renderer and Scene are owned by BaseExample - BaseExample::Shutdown()
	// (below) deletes and NULLs them itself.
	BaseExample::Shutdown();
}

TextRendering::~TextRendering() {}
