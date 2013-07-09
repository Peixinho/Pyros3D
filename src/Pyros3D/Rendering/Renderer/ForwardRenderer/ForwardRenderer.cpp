//============================================================================
// Name        : ForwardRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Forward Renderer
//============================================================================

#include "ForwardRenderer.h"
#include <GL/glew.h>

namespace p3d {
    
    ForwardRenderer::ForwardRenderer(const uint32& Width, const uint32& Height) : IRenderer(Width,Height) 
    {
    
        ActivateCulling(CullingMode::FrustumCulling);
    
    }
    
    ForwardRenderer::~ForwardRenderer()
    {
        if (IsCulling)
        {
            delete culling;
        }
    }
    
    void ForwardRenderer::GroupAndSortAssets()
    {
        
        // Sort and Group Objects From Scene
         
    }
    
    void ForwardRenderer::RenderScene(const Matrix& Projection, GameObject* Camera, SceneGraph* Scene)
    {
        // Update Culling
        UpdateCulling(Camera->GetWorldTransformation().Inverse(),Projection);
        
        // Save Values for Cache
        // Saves Scene
        this->Scene = Scene;
        
        // Saves Camera
        this->Camera = Camera;
        this->CameraPosition = this->Camera->GetPosition();
        
        // Saves Projection
        this->Projection = Projection;
        
        // Universal Cache
        ProjectionMatrix = Projection;
        NearFarPlane = Vec2(Projection.Near, Projection.Far);
        
        // View Matrix and Position
        ViewMatrix = Camera->GetWorldTransformation().Inverse();
        CameraPosition = Camera->GetPosition();
        
        // Flags
        ViewMatrixInverseIsDirty = true;
        ProjectionMatrixInverseIsDirty = true;
        ViewProjectionMatrixIsDirty = true;
        
        // Get Rendering Components List
        std::vector<RenderingMesh*> rmesh = RenderingComponent::GetRenderingMeshes(Scene);
        
        // Get Lights List
        std::vector<IComponent*> lcomps = ILightComponent::GetComponentsOnScene(Scene);
        
        // Pack Lights
        Lights.clear();
        if (lcomps.size()>0) 
        {
            for (std::vector<IComponent*>::iterator i = lcomps.begin();i!=lcomps.end();i++)
            {
                if (DirectionalLight* d = dynamic_cast<DirectionalLight*>((*i))) {
                    
                    // Directional Lights
                    Vec4 color = d->GetLightColor();
                    Vec3 position;
                    Vec3 LDirection = d->GetOwner()->GetWorldPosition().normalize();
                    Vec4 direction = ViewMatrix * Vec4(LDirection.x,LDirection.y,LDirection.z,0.f);
                    float attenuation = 1.f;
                    Vec2 cones;
                    int type = 1;
                    
                    Matrix directionalLight = Matrix();
                    directionalLight.m[0] = color.x;         directionalLight.m[1] = color.y;             directionalLight.m[2] = color.z;             directionalLight.m[3] = color.w;
                    directionalLight.m[4] = position.x;      directionalLight.m[5] = position.y;          directionalLight.m[6] = position.z; 
                    directionalLight.m[7] = direction.x;     directionalLight.m[8] = direction.y;         directionalLight.m[9] = direction.z;
                    directionalLight.m[10] = attenuation;   //directionalLight.m[11] = attenuation.y;       directionalLight.m[12] = attenuation.z;
                    directionalLight.m[13] = cones.x;         directionalLight.m[14] = cones.y;
                    directionalLight.m[15] = type;
                    
                    Lights.push_back(directionalLight);
                    
                } else if (PointLight* p = dynamic_cast<PointLight*>((*i))) {
                    
                    // Point Lights
                    Vec4 color = p->GetLightColor();
                    Vec3 position = ViewMatrix * (p->GetOwner()->GetWorldPosition());
                    Vec3 direction;
                    float attenuation = p->GetLightRadius();
                    Vec2 cones;
                    int type = 2;
                    
                    Matrix pointLight = Matrix();
                    pointLight.m[0] = color.x;       pointLight.m[1] = color.y;           pointLight.m[2] = color.z;           pointLight.m[3] = color.w;
                    pointLight.m[4] = position.x;    pointLight.m[5] = position.y;        pointLight.m[6] = position.z; 
                    pointLight.m[7] = direction.x;   pointLight.m[8] = direction.y;       pointLight.m[9] = direction.z;
                    pointLight.m[10] = attenuation; //pointLight.m[11] = attenuation.y;     pointLight.m[12] = attenuation.z;
                    pointLight.m[13] = cones.x;       pointLight.m[14] = cones.y;
                    pointLight.m[15] = type;
                    
                    Lights.push_back(pointLight);
                    
                } else if (SpotLight* s = dynamic_cast<SpotLight*>((*i))) {
                    
                    // Spot Lights
                    Vec4 color = s->GetLightColor();
                    Vec3 position = ViewMatrix * (s->GetOwner()->GetWorldPosition());                   
                    Vec3 LDirection = d->GetOwner()->GetWorldPosition().normalize();
                    Vec4 direction = ViewMatrix * Vec4(LDirection.x,LDirection.y,LDirection.z,0.f);
                    float attenuation = s->GetLightRadius();
                    Vec2 cones = Vec2(s->GetLightCosInnerCone(),s->GetLightCosOutterCone());
                    int type = 3;
                    
                    Matrix spotLight = Matrix();
                    spotLight.m[0] = color.x;        spotLight.m[1] = color.y;            spotLight.m[2] = color.z;            spotLight.m[3] = color.w;
                    spotLight.m[4] = position.x;     spotLight.m[5] = position.y;         spotLight.m[6] = position.z; 
                    spotLight.m[7] = direction.x;    spotLight.m[8] = direction.y;        spotLight.m[9] = direction.z;
                    spotLight.m[10] = attenuation;  //spotLight.m[11] = attenuation.y;      spotLight.m[12] = attenuation.z;
                    spotLight.m[13] = cones.x;        spotLight.m[14] = cones.y;
                    spotLight.m[15] = type;
                    
                    Lights.push_back(spotLight);
                    
                }
            }
        }
        
        // Update Lights Position and Direction to ViewSpace
        NumberOfLights = Lights.size();
        
        // Group and Sort Meshes
        GroupAndSortAssets();
        
        // Set ViewPort
        glViewport(0,0,Width,Height);
        
        // Clear Screen
        ClearScreen(Buffer_Bit::Color | Buffer_Bit::Depth);
        SetBackground(Vec4::ZERO);
        EnableDepthTest();
        
        // Enable Depth Bias
        if (IsUsingDepthBias)
        {
            glEnable(GL_DEPTH_BIAS);
            glPolygonOffset(DepthBias.x,DepthBias.y);
        }
        
        // Flags
        LastProgramUsed = -1;
        InternalDrawType = -1;
        
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
                        cullingTest =CullingSphereTest(*i);
                        break;
                }
                if (cullingTest)
                    //RenderObject((*i),(Material!=NULL?Material:(*i)->Material)); // If it had a material default defined like cast shadows
                    RenderObject((*i),(*i)->Material);
            }
        }
        
        // Disable Cull Face
        glDisable(GL_CULL_FACE);
        
        // Set Default Polygon Mode
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        
        // Unbind Material
        glUseProgram(0);
        
        // Disable Depth Bias
        if (IsUsingDepthBias) glDisable(GL_DEPTH_BIAS);
    }
    
};