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
    
    DeferredRenderer::DeferredRenderer(const uint32& Width, const uint32& Height) : IRenderer(Width,Height) 
    {
    
        echo("SUCCESS: Deferred Renderer Created");
        
        ActivateCulling(CullingMode::FrustumCulling);

        shadowMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows);
        shadowMaterial->SetCullFace(CullFace::DoubleSided);
        shadowSkinnedMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::Skinning);
        shadowSkinnedMaterial->SetCullFace(CullFace::DoubleSided);

        // Default View Port Init Values
        viewPortStartX = viewPortStartY = 0;
    }
    
    DeferredRenderer::~DeferredRenderer()
    {
        if (IsCulling)
        {
            delete culling;
        }
        
        delete shadowMaterial;
    }
    
    
    
    std::vector<RenderingMesh*> DeferredRenderer::GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera)
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

        // Set ViewPort
        if (viewPortEndX==0 || viewPortEndY==0) 
        { 
            viewPortEndX = Width;
            viewPortEndY = Height;
        }
        
        _SetViewPort(viewPortStartX,viewPortStartY,viewPortEndX,viewPortEndY);

        // Bind Frame Buffer
        FBO->Bind();

        // Clear Screen
        ClearScreen(BufferOptions);
        
        // Draw Background
        DrawBackground();
        
        // Depth Test
        RunDepthTest();

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
                if (cullingTest && (*i)->renderingComponent->IsActive() && (*i)->Active == true)
                    RenderObject((*i),(*i)->Material);
            }
        }
        // Disable Cull Face
        glDisable(GL_CULL_FACE);

#ifndef ANDROID
        // Set Default Polygon Mode
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

        // End Rendering
        EndRender();

        // Unbind FrameBuffer
        FBO->UnBind();

    }

    void DeferredRenderer::SetFBO(FrameBuffer* fbo)
    {
        FBO = fbo;
    }
    
};
