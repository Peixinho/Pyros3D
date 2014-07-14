//============================================================================
// Name        : DeferredRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Deferred Renderer
//============================================================================

#include "DeferredRenderer.h"
#ifdef ANDROID
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
#else
    #include "GL/glew.h"
#endif

namespace p3d {
    
    DeferredRenderer::DeferredRenderer(const uint32& Width, const uint32& Height, FrameBuffer* fbo) : IRenderer(Width,Height) 
    {
    
        echo("SUCCESS: Deferred Renderer Created");
        
        ActivateCulling(CullingMode::FrustumCulling);

        shadowMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows);
        shadowMaterial->SetCullFace(CullFace::DoubleSided);
        shadowSkinnedMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::Skinning);
        shadowSkinnedMaterial->SetCullFace(CullFace::DoubleSided);

        // Default View Port Init Values
        viewPortStartX = viewPortStartY = 0;

        // Save FrameBuffer
        FBO = fbo;

        // Create Second Pass Specifics
        uint32 texID = 0;
        deferredMaterial = new CustomShaderMaterial("../../../../examples/DeferredRendering/assets/shaders/secondpass.vert","../../../../examples/DeferredRendering/assets/shaders/secondpass.frag");
        deferredMaterial->AddUniform(Uniform::Uniform("tDepth", Uniform::DataType::Int, &texID));
        texID = 1;
        deferredMaterial->AddUniform(Uniform::Uniform("tDiffuse", Uniform::DataType::Int, &texID));
        texID = 2;
        deferredMaterial->AddUniform(Uniform::Uniform("tSpecular", Uniform::DataType::Int, &texID));
        texID = 3;
        deferredMaterial->AddUniform(Uniform::Uniform("tNormal", Uniform::DataType::Int, &texID));
        texID = 4;
        deferredMaterial->AddUniform(Uniform::Uniform("tPosition", Uniform::DataType::Int, &texID));
        
        deferredMaterial->AddUniform(Uniform::Uniform("uScreenDimensions", Uniform::DataUsage::ScreenDimensions));
        deferredMaterial->AddUniform(Uniform::Uniform("uLightPosition", Uniform::DataUsage::Other, Uniform::DataType::Vec3));
        deferredMaterial->AddUniform(Uniform::Uniform("uLightRadius", Uniform::DataUsage::Other, Uniform::DataType::Float));
        deferredMaterial->AddUniform(Uniform::Uniform("uLightColor", Uniform::DataUsage::Other, Uniform::DataType::Vec4));
        deferredMaterial->AddUniform(Uniform::Uniform("uModelMatrix", Uniform::DataUsage::ModelMatrix));
        deferredMaterial->AddUniform(Uniform::Uniform("uViewMatrix", Uniform::DataUsage::ViewMatrix));
        deferredMaterial->AddUniform(Uniform::Uniform("uProjectionMatrix", Uniform::DataUsage::ProjectionMatrix));
        
        // Light Volume
        sphereHandle = new Sphere(1,4,4);
        pointLight = new RenderingComponent(sphereHandle);
    }
    
    void DeferredRenderer::Resize(const uint32& Width, const uint32& Height)
    {
        IRenderer::Resize(Width,Height);
    }
    
    DeferredRenderer::~DeferredRenderer()
    {
        if (IsCulling)
        {
            delete culling;
        }
        
        delete shadowMaterial;
        delete sphereHandle;
    }
    
    std::vector<RenderingMesh*> DeferredRenderer::GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 &Tag)
    {
        
        // Sort and Group Objects From Scene
        std::vector<RenderingMesh*> _OpaqueMeshes;
        std::map<f32,RenderingMesh*> _TranslucidMeshes;
        
        std::vector<RenderingMesh*> rmeshes = RenderingComponent::GetRenderingMeshes(Scene);
        
        for (std::vector<RenderingMesh*>::iterator k=rmeshes.begin();k!=rmeshes.end();k++)
        {
            if ((*k)->Material->IsTransparent())
            {
                f32 index = Camera->GetPosition().distanceSQR((*k)->renderingComponent->GetOwner()->GetWorldPosition());
                while(_TranslucidMeshes.find(index)!=_TranslucidMeshes.end()) index+=1.f;
                _TranslucidMeshes[index] = (*k);
            }
            else _OpaqueMeshes.push_back((*k));
        }
        
        for (std::map<f32,RenderingMesh*>::iterator i=_TranslucidMeshes.begin();i!=_TranslucidMeshes.end();i++)
        {
            _OpaqueMeshes.push_back((*i).second);
        }
        
        return _OpaqueMeshes;
         
    }
    
    void DeferredRenderer::RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const uint32 BufferOptions)
    {
        
        // Initialize Renderer
        InitRender();
        
        // Group and Sort Meshes
        std::vector<RenderingMesh*> rmesh = GroupAndSortAssets(Scene, Camera);
        
        // Get Lights List
        std::vector<IComponent*> lcomps = ILightComponent::GetLightsOnScene(Scene);
        
        // Save Time
        Timer = Scene->GetTime();

        // First Pass

            // Save Values for Cache
            // Saves Scene
            this->Scene = Scene;

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

            // Update Culling
            UpdateCulling(ProjectionMatrix*ViewMatrix);

            // Flags
            ViewMatrixInverseIsDirty = true;
            ProjectionMatrixInverseIsDirty = true;
            ViewProjectionMatrixIsDirty = true;

            // Bind Frame Buffer
            FBO->Bind();

            // Set ViewPort
            viewPortEndX = Width;
            viewPortEndY = Height;
            // Set Viewport
            _SetViewPort(viewPortStartX,viewPortStartY,viewPortEndX,viewPortEndY);
            ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
            DepthTest();
            DepthWrite();
            ClearDepthBuffer();
            ClearScreen();

            // Disable Blending
            DisableBlending();

            // Draw Background
            DrawBackground();

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
                            cullingTest = CullingBoxTest((*i),(*i)->renderingComponent->GetOwner());
                            break;
                        case CullingGeometry::Sphere:
                        default:
                            cullingTest = CullingSphereTest((*i), (*i)->renderingComponent->GetOwner());
                            break;
                    }
                    if (cullingTest && (*i)->renderingComponent->IsActive() && (*i)->Active == true)
                        RenderObject((*i),(*i)->renderingComponent->GetOwner(),(*i)->Material);
                }
            }

            // End Rendering
            EndRender();

            // Initialize Rendering
            InitRender();

            // Unbind FrameBuffer
            FBO->UnBind();

            // Disable Depth Masking
            DisableDepthWritting();

            // Disable Depth Test
            DisableDepthTest();

            // Second Pass
            EnableBlending();
            BlendingEquation(BlendEq::Add);
            BlendingFunction(BlendFunc::One, BlendFunc::One);

            ClearBufferBit(Buffer_Bit::Color);
            DepthTest();
            DepthWrite();
            ClearDepthBuffer();
            ClearScreen();

            // Bind FBO Textures
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Depth_Attachment]->TexturePTR->Bind();
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Color_Attachment0]->TexturePTR->Bind();
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Color_Attachment1]->TexturePTR->Bind();
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Color_Attachment2]->TexturePTR->Bind();
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Color_Attachment3]->TexturePTR->Bind();

            // Render Point Lights
            for (std::vector<IComponent*>::iterator i=lcomps.begin();i!=lcomps.end();i++)
            {

                if ((*i)->GetOwner()!=NULL)
                {
                    if (PointLight* p = dynamic_cast<PointLight*>((*i))) {
                        // Point Lights
                            // Set Scale
                            Vec3 pos = p->GetOwner()->GetWorldPosition();
                            Vec4 color = p->GetLightColor();
                            deferredMaterial->SetUniformValue("uLightPosition", &pos);
                            deferredMaterial->SetUniformValue("uLightRadius", p->GetLightRadius());
                            deferredMaterial->SetUniformValue("uLightColor", &color);
                            pointLight->GetMeshes()[0]->Pivot.Scale(p->GetLightRadius(),p->GetLightRadius(),p->GetLightRadius());
                            RenderObject(pointLight->GetMeshes()[0],p->GetOwner(),deferredMaterial);
                    }
                    else if (SpotLight* s = dynamic_cast<SpotLight*>((*i))) {
                        // Spot Lights


                    }
                    else if (DirectionalLight* d = dynamic_cast<DirectionalLight*>((*i))) {
                        // Directional Lights


                    }
                }
            }
        
            // Enable Depth Test
            EnableDepthTest();
            EnableDepthWritting();
            
            // Disable Blending
            DisableBlending();

            FBO->GetAttachments()[FrameBufferAttachmentFormat::Color_Attachment3]->TexturePTR->Unbind();
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Color_Attachment2]->TexturePTR->Unbind();
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Color_Attachment1]->TexturePTR->Unbind();
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Color_Attachment0]->TexturePTR->Unbind();
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Depth_Attachment]->TexturePTR->Unbind();

            // End Render
            EndRender();
    }

    void DeferredRenderer::SetFBO(FrameBuffer* fbo)
    {
        // Save FBO
        FBO = fbo;
    }
    
};
