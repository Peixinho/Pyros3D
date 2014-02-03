//============================================================================
// Name        : TextRendering.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Text Rendering Example
//============================================================================

#include "TextRendering.h"
#include "Pyros3D/Utils/Colors/Colors.h"
#include "Pyros3D/AssetManager/Assets/Font/Font.h"

using namespace p3d;

TextRendering::TextRendering() : ClassName(1024,768,"Pyros3D - Text Rendering",WindowType::Close | WindowType::Resize)
{
    
}

void TextRendering::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Ortho(0.f,(f32)Width,0.f,(f32)Height,0.f,10.f);
}

void TextRendering::Init()
{
    // Initialization
    
        // Initialize Scene
        Scene = new SceneGraph();
        
        // Initialize Renderer
        Renderer = new ForwardRenderer(Width,Height);

        // Projection
        projection.Ortho(0.f,Width,0.f,Height,0.f,10.f);
        
        // Create Camera
        Camera = new GameObject();
        Camera->SetPosition(Vec3(0,0,1));
        
        // Add Camera to Scene
        Scene->Add(Camera);
        
        // Create Font
        font = AssetManager::CreateFont("../../../../examples/TextRendering/verdana.ttf",16);
        font->CreateText("aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ,.0123456789[]()!?+-_\\|/ºª");
        
        // Create Text Material
        textMaterial = new GenericShaderMaterial(ShaderUsage::TextRendering);
        textMaterial->SetTextFont(font);
        // Set Material Font to use Font Map
        
        // Create Game Object
        Text = new GameObject();
        rText = new RenderingComponent(AssetManager::CreateText(font,"Pyros3D Engine - Text Rendering\n\n:)",16,16,Vec4(1,1,1,1)),textMaterial);
        Text->Add(rText);
        
        // Add GameObject to Scene
        Scene->Add(Text);

}

void TextRendering::Update()
{
    // Update - Game Loop
        
        // Update Scene
        Scene->Update(GetTime());
        
        // Game Logic Here
        Text->SetRotation(Vec3(0,0,GetTime()));
        Text->SetPosition(Vec3(Width/2.f,Height/2.f,0.f));

        // Render Scene
        Renderer->RenderScene(projection,Camera,Scene);
}

void TextRendering::Shutdown()
{
    // All your Shutdown Code Here
    
        // Remove GameObjects From Scene
        Scene->Remove(Text);
        Scene->Remove(Camera);
        
        Text->Remove(rText);
    
        // Delete
        delete rText;
        delete Text;
        delete Camera;
        delete Renderer;
        delete Scene;
}

TextRendering::~TextRendering() {}
