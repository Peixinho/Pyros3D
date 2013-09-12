//============================================================================
// Name        : PainterPick.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Painter Pick Class
//============================================================================

#include "PainterPick.h"

namespace p3d {

    PainterPick::PainterPick(const uint32& Width, const uint32& Height): IRenderer(Width,Height) {
        
        // Create Material
        material = new GenericShaderMaterial(ShaderUsage::Color);
        // Frame Buffer
        fbo = new FrameBuffer();
        // Texture Creation
        textureHandle = AssetManager::CreateTexture(TextureType::Texture,TextureDataType::RGBA,Width,Height,false);
        texture = static_cast<Texture*>(AssetManager::GetAsset(textureHandle)->AssetPTR);
        // Frame Buffer Creation
        fbo->Init(FrameBufferAttachmentFormat::Color_Attachment0,TextureType::Texture,texture,true);
        
        // Activate Culling
        ActivateCulling(CullingMode::FrustumCulling);
    }
    void PainterPick::Resize(const uint32& Width, const uint32& Height)
    {
        IRenderer::Resize(Width,Height);
        AssetManager::Resize(textureHandle,Width,Height);
        fbo->ResizeRenderBuffer(Width,Height);
    }
    PainterPick::~PainterPick() 
    {
        // Delete Material
        delete material;
        // Delete FBO
        delete fbo;
        // Delete Texture
        AssetManager::DeleteAsset(textureHandle);
    }

    RenderingMesh* PainterPick::PickObject(const f32& mouseX, const f32& mouseY, Projection projection, GameObject* Camera, SceneGraph* Scene)
    {
        this->mouseX = mouseX;
        this->mouseY = mouseY;
        this->Scene = Scene;
        
        RenderScene(projection,Camera,Scene);
        
        // Get Texture Color
        uint8 color[4];
        memcpy(&color,&texture->GetTextureData()[uint32((mouseX*4)+((Height-mouseY)*Width*4))],sizeof(uint8)*4);
        
        Vec4 pixel = Vec4((int32)color[0]/255.f,(int32)color[1]/255.f,(int32)color[2]/255.f,(int32)color[3]/255.f);
        f32 colorPointer = Vec4ToRgba8(pixel);
        if (MeshPickingList.find(colorPointer)!=MeshPickingList.end())
        {
            return MeshPickingList[colorPointer];
        }
        else return NULL;
    }
    
    void PainterPick::RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene)
    {
        // Colors
        colors = 0;
        
        // Clear MeshPainter List
        MeshPickingList.clear();
        
        // Saves Camera
        this->Camera = Camera;
        this->CameraPosition = this->Camera->GetWorldPosition();
        
        // Saves Projection
        this->projection = projection;
        
        // Universal Cache
        ProjectionMatrix = projection.m;
        NearFarPlane = Vec2(projection.Near, projection.Far);
        
        // View Matrix and Position
        ViewMatrix = Camera->GetWorldTransformation().Inverse();
        CameraPosition = Camera->GetWorldPosition();

        // Flags
        ViewMatrixInverseIsDirty = true;
        ProjectionMatrixInverseIsDirty = true;
        ViewProjectionMatrixIsDirty = true;
        
        UpdateCulling(ViewMatrix,projection);
        
        // Initialize Rendering
        InitRender();
        
        // Get Rendering Components List
        std::vector<RenderingMesh*> rmesh = RenderingComponent::GetRenderingMeshes(Scene);
        
        // Bind FBO
        fbo->Bind();
        
        // Set ViewPort
        SetViewPort(0,0,Width,Height);
        
        // Clear Screen
        ClearScreen(Buffer_Bit::Color | Buffer_Bit::Depth);
        SetBackground(Vec4(0,0,0,0));
        EnableDepthTest();
        
        // Render Scene with Objects Material
        for (std::vector<RenderingMesh*>::iterator i=rmesh.begin();i!=rmesh.end();i++)
        {
            
            if ((*i)->renderingComponent->GetOwner()!=NULL)
            {
                // Culling Test
                bool cullingTest = false;
                switch((*i)->CullingGeometry)
                {
                    case CullingGeometry::Box:
                        cullingTest = CullingBoxTest(*i);
                        break;
                    case CullingGeometry::Sphere:
                    default:
                        cullingTest = CullingSphereTest(*i);
                        break;
                }
                if (cullingTest)
                {
                    colors++;
                    Vec4 color = Rgba8ToVec4(colors);
                    material->SetColor(color);
                    RenderObject((*i),material);
                    MeshPickingList[colors]=(*i);
                }
            }
        }
        
        // End Rendering
        EndRender();
        
        // Unbind FBO
        fbo->UnBind();
    }
}