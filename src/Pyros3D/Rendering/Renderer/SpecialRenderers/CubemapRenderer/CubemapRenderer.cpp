//============================================================================
// Name        : CubemapRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Dynamic Cube Map aka Environment Map
//============================================================================

#include "CubemapRenderer.h"
#include <GL/glew.h>

namespace p3d {
    
    CubemapRenderer::CubemapRenderer(const uint32& Width, const uint32& Height) : IRenderer(Width,Height) 
    {
    
        echo("SUCCESS: Forward Renderer Created");
        
        ActivateCulling(CullingMode::FrustumCulling);
        
        // Create Texture (CubeMap), Frame Buffer and Set the Texture as Attachment
        environmentMap=AssetManager::CreateTexture(TextureType::CubemapNegative_X,TextureDataType::RGB,Width,Height,false);
        environmentMap->CreateTexture(TextureType::CubemapNegative_Y,TextureDataType::RGB,Width,Height,false);
        environmentMap->CreateTexture(TextureType::CubemapNegative_Z,TextureDataType::RGB,Width,Height,false);
        environmentMap->CreateTexture(TextureType::CubemapPositive_X,TextureDataType::RGB,Width,Height,false);
        environmentMap->CreateTexture(TextureType::CubemapPositive_Y,TextureDataType::RGB,Width,Height,false);
        environmentMap->CreateTexture(TextureType::CubemapPositive_Z,TextureDataType::RGB,Width,Height,false);
        environmentMap->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);

        // Initialize Frame Buffer
        fbo = new FrameBuffer();
        fbo->Init(FrameBufferAttachmentFormat::Color_Attachment0,TextureType::CubemapPositive_X,environmentMap,true);
        
    }
    
    CubemapRenderer::~CubemapRenderer()
    {
        if (IsCulling)
        {
            delete culling;
        }
        
        AssetManager::DeleteTexture(environmentMap);
        delete fbo;
    }
    
    
    
    std::vector<RenderingMesh*> CubemapRenderer::GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera)
    {
        
        // Sort and Group Objects From Scene
        std::vector<RenderingMesh*> _OpaqueMeshes;
        std::map<f32,RenderingMesh*> _TranslucidMeshes;
        
        std::vector<RenderingMesh*> rmeshes = RenderingComponent::GetRenderingMeshes(Scene);
        
        for (std::vector<RenderingMesh*>::iterator k=rmeshes.begin();k!=rmeshes.end();k++)
        {
            if ((*k)->Material->IsTransparent())
            {
                f32 index = Camera->GetWorldPosition().distanceSQR((*k)->renderingComponent->GetOwner()->GetWorldPosition());
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
    
    void CubemapRenderer::RenderCubeMap(SceneGraph* Scene, GameObject* AllSeeingEye, const f32 &Near, const f32 &Far)
    {
        
        InitRender();
        
        this->AllSeeingEye = AllSeeingEye;
        ProjectionMatrix = Matrix::PerspectiveMatrix(90.f, 1.0, Near, Far);
        
        // Universal Cache
        NearFarPlane = Vec2(Near, Far);
        CameraPosition = AllSeeingEye->GetWorldPosition();

        // Flags
        ViewMatrixInverseIsDirty = true;
        ProjectionMatrixInverseIsDirty = true;
        ViewProjectionMatrixIsDirty = true;
        
        // Group and Sort Meshes
        std::vector<RenderingMesh*> rmesh = GroupAndSortAssets(Scene,AllSeeingEye);
        
        // Get Lights List
        std::vector<IComponent*> lcomps = ILightComponent::GetLightsOnScene(Scene);
        
        if (rmesh.size()>0)
        {
            
            // Prepare and Pack Lights to Send to Shaders
            Lights.clear();
         
            // ShadowMaps
            DirectionalShadowMapsTextures.clear();
            DirectionalShadowMatrix.clear();
            NumberOfDirectionalShadows = 0;
            
            PointShadowMapsTextures.clear();
            PointShadowMatrix.clear();
            NumberOfPointShadows = 0;
            
            SpotShadowMapsTextures.clear();
            SpotShadowMatrix.clear();
            NumberOfSpotShadows = 0;
            
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
                        f32 attenuation = 1.f;
                        Vec2 cones;
                        int32 type = 1;
                        
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
                        f32 attenuation = p->GetLightRadius();
                        Vec2 cones;
                        int32 type = 2;
                        
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
                        Vec3 LDirection = s->GetLightDirection().normalize();
                        Vec4 direction = ViewMatrix * Vec4(LDirection.x,LDirection.y,LDirection.z,0.f);
                        f32 attenuation = s->GetLightRadius();
                        Vec2 cones = Vec2(s->GetLightCosInnerCone(),s->GetLightCosOutterCone());
                        int32 type = 3;
                        
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
        
            // Save Time
            Timer = Scene->GetTime();
        
            // Update Lights Position and Direction to ViewSpace
            NumberOfLights = Lights.size();
            
            // Bind FBO
            fbo->Bind();
        
            for (uint32 i=0;i<6;i++)
            {
                // Clean View Matrix
                ViewMatrix.identity();

                // Create Light View Matrix For Rendering Each Face of the Cubemap
                if (i==0)
                    ViewMatrix.LookAt(Vec3::ZERO, Vec3(1.0, 0.0, 0.0), Vec3(0.0,-1.0,0.0)); // +X
                if (i==1)
                    ViewMatrix.LookAt(Vec3::ZERO, Vec3(-1.0, 0.0, 0.0), Vec3(0.0,-1.0,0.0)); // -X
                if (i==2)
                    ViewMatrix.LookAt(Vec3::ZERO, Vec3(0.0, 1.0, 0.0), Vec3(0.0,0.0,1.0)); // +Y
                if (i==3)
                    ViewMatrix.LookAt(Vec3::ZERO, Vec3(0.0, -1.0, 0.0), Vec3(0.0,0.0,-1.0)); // -Y
                if (i==4)
                    ViewMatrix.LookAt(Vec3::ZERO, Vec3(0.0, 0.0, 1.0), Vec3(0.0,-1.0,0.0)); // +Z
                if (i==5)
                    ViewMatrix.LookAt(Vec3::ZERO, Vec3(0.0, 0.0, -1.0), Vec3(0.0,-1.0,0.0)); // -Z
        
                // Translate Light View Matrix
                ViewMatrix*=AllSeeingEye->GetWorldTransformation().Inverse();
            
                // Frame Buffer Attachment
                fbo->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0,TextureType::CubemapPositive_X+i,environmentMap);

                // Set Viewport
                _SetViewPort(0,0, Width, Height);
            
                // Clear Screen
                ClearScreen(Buffer_Bit::Color | Buffer_Bit::Depth);
                
                // Draw Background
                DrawBackground();
                
                // Run Depth Test
                RunDepthTest();
                
                // Update Culling
                UpdateCulling(ProjectionMatrix*ViewMatrix);
                
                // Render Scene with Objects Material
                for (std::vector<RenderingMesh*>::iterator k=rmesh.begin();k!=rmesh.end();k++)
                {

                    if ((*k)->renderingComponent->GetOwner()!=NULL)
                    {
                        // Culling Test
                        bool cullingTest = false;
                        switch((*k)->CullingGeometry)
                        {
                            case CullingGeometry::Box:
                                cullingTest = CullingBoxTest(*k);
                                break;
                            case CullingGeometry::Sphere:
                            default:
                                cullingTest = CullingSphereTest(*k);
                                break;
                        }
                        if (cullingTest && (*k)->renderingComponent->IsActive())
                            RenderObject((*k),(*k)->Material);
                    }
                }
            }
        
            fbo->UnBind();
        
            EndRender();
        }
    }
    
    Texture* CubemapRenderer::GetTexture()
    {
        return environmentMap;
    }
    
};