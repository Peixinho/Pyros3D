//============================================================================
// Name        : DeferredRenderingExample.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Example
//============================================================================

#include "DeferredRenderingExample.h"

using namespace p3d;

DeferredRenderingExample::DeferredRenderingExample() : ClassName(1024,768,"Pyros3D - Deferred Rendering Example",WindowType::Close | WindowType::Resize)
{
    
}

void DeferredRenderingExample::OnResize(const uint32 &width, const uint32 &height)
{
    // Execute Parent Resize Function
    ClassName::OnResize(width, height);
    
    // Resize
    Renderer->Resize(width, height);
    projection.Perspective(70.f,(f32)width/(f32)height,1.f,200.f);

    albedoTexture->Resize(Width, Height);
    specularTexture->Resize(Width, Height);
    normalTexture->Resize(Width, Height);
    depthTexture->Resize(Width, Height);
    positionTexture->Resize(Width, Height);
}

void DeferredRenderingExample::Init()
{
    // Initialization
    
        // Initialize Scene
        Scene = new SceneGraph();
        
        // Setting Deferred Rendering Framebuffer and Textures
        albedoTexture = AssetManager::CreateTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height);
        specularTexture = AssetManager::CreateTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height);
        normalTexture = AssetManager::CreateTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height);
        positionTexture = AssetManager::CreateTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height);
        depthTexture = AssetManager::CreateTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height);

        albedoTexture->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);
        specularTexture->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);
        normalTexture->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);
        depthTexture->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);
        positionTexture->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);

        deferredFBO = new FrameBuffer();
        deferredFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, depthTexture);
        deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, albedoTexture);
        deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment1, TextureType::Texture, specularTexture);
        deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment2, TextureType::Texture, normalTexture);
        deferredFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment3, TextureType::Texture, positionTexture);

        // Initialize Renderer
        Renderer = new DeferredRenderer(Width, Height, deferredFBO);

        // Projection
        projection.Perspective(70.f,(f32)Width/(f32)Height,1.f,200.f);
        
        // Create Camera
        Camera = new GameObject();
        Camera->SetPosition(Vec3(0,10,80));

        // Create 100 Point Lights
        for (uint32 i=0;i<100;i++)
        {
            // Create Light GameObject
            GameObject* Light = new GameObject();
            // Add Light to GameObjects List
            Lights.push_back(Light);
            // Create Point Light
            PointLight* pLight = new PointLight(Vec4((rand() % 100)/100.f,(rand() % 100)/100.f,(rand() % 100)/100.f,1.0), 50.f);
            // Add Rendering Component to Rendering Components List
            pLights.push_back(pLight);
            // Add Point Light Component to GameObject
            Light->Add(pLight);
            // Add GameObject to Scene
            Scene->Add(Light);
            // Set Random Position to the GameObject
            Light->SetPosition(Vec3((rand() % 100) -50,(rand() % 100)-50,(rand() % 100) -50));
        }

        // Material
        Diffuse = new CustomShaderMaterial("../../../../examples/DeferredRendering/shaders/gbuffer.vert","../../../../examples/DeferredRendering/shaders/gbuffer.frag");        
        Diffuse->AddUniform(Uniform::Uniform("uModelMatrix", Uniform::DataUsage::ModelMatrix));
        Diffuse->AddUniform(Uniform::Uniform("uViewMatrix", Uniform::DataUsage::ViewMatrix));
        Diffuse->AddUniform(Uniform::Uniform("uProjectionMatrix", Uniform::DataUsage::ProjectionMatrix));
        Diffuse->AddUniform(Uniform::Uniform("uColor", Uniform::DataUsage::Other, Uniform::DataType::Vec4));
        Vec4 color = Vec4(0.8,0.8,0.8,1.0);
        Diffuse->SetUniformValue("uColor", &color);

        // Create Geometry
        Renderable* cubeHandle = AssetManager::CreateCube(10,10,10);

        // Create 100 Cubes
        for (uint32 i=0;i<100;i++)
        {
            // Create GameObject
            GameObject* Cube = new GameObject();
            // Add Cube to GameObjects List
            Cubes.push_back(Cube);
            // Create Rendering Component using Geometry Previously Created with AssetManager
            RenderingComponent* rCube = new RenderingComponent(cubeHandle, Diffuse);
            // Add Rendering Component to Rendering Components List
            rCubes.push_back(rCube);
            // Add Rendering Component to GameObject
            Cube->Add(rCube);
            // Add GameObject to Scene
            Scene->Add(Cube);
            // Set Random Position to the GameObject
            Cube->SetPosition(Vec3((rand() % 100) -50,(rand() % 100)-50,(rand() % 100) -50));
        }

        // Add Camera to Scene
        Scene->Add(Camera);
        Camera->LookAt(Vec3::ZERO);
}

void DeferredRenderingExample::Update()
{
    // Update - Game Loop
    
    // Create 100 Cubes
    for (uint32 i=0;i<100;i++)
    {
        Cubes[i]->SetRotation(Vec3(0,GetTime(),0));
    }

    // Update Scene
    Scene->Update(GetTime());

    // Render Scene
    Renderer->RenderScene(projection,Camera,Scene);
}

void DeferredRenderingExample::Shutdown()
{
    // All your Shutdown Code Here

}

DeferredRenderingExample::~DeferredRenderingExample() {}
