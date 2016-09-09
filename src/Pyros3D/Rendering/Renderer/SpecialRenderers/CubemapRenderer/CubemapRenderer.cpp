//============================================================================
// Name        : CubemapRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Dynamic Cube Map aka Environment Map
//============================================================================

#include <Pyros3D/Rendering/Renderer/SpecialRenderers/CubemapRenderer/CubemapRenderer.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {
    
    CubemapRenderer::CubemapRenderer(const uint32 Width, const uint32 Height) : IRenderer(Width,Height) 
    {
    
        echo("SUCCESS: Forward Renderer Created");
        
        ActivateCulling(CullingMode::FrustumCulling);
        
        // Create Texture (CubeMap), Frame Buffer and Set the Texture as Attachment
        environmentMap = new Texture();
        environmentMap->CreateEmptyTexture(TextureType::CubemapNegative_X,TextureDataType::RGB,Width,Height,false);
        environmentMap->CreateEmptyTexture(TextureType::CubemapNegative_Y,TextureDataType::RGB,Width,Height,false);
        environmentMap->CreateEmptyTexture(TextureType::CubemapNegative_Z,TextureDataType::RGB,Width,Height,false);
        environmentMap->CreateEmptyTexture(TextureType::CubemapPositive_X,TextureDataType::RGB,Width,Height,false);
        environmentMap->CreateEmptyTexture(TextureType::CubemapPositive_Y,TextureDataType::RGB,Width,Height,false);
        environmentMap->CreateEmptyTexture(TextureType::CubemapPositive_Z,TextureDataType::RGB,Width,Height,false);
        environmentMap->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);

        // Initialize Frame Buffer
        fbo = new FrameBuffer();
		fbo->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, Width, Height);
        fbo->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0,TextureType::CubemapPositive_X,environmentMap);
        
    }
    
    CubemapRenderer::~CubemapRenderer()
    {
        if (IsCulling)
        {
            delete culling;
        }
        
        delete environmentMap;
        delete fbo;
    }
    
    std::vector<RenderingMesh*> CubemapRenderer::GroupAndSortAssets(SceneGraph* Scene, const Vec3 &CameraPos, const uint32 Tag)
    {
        
        // Sort and Group Objects From Scene
        std::vector<RenderingMesh*> _OpaqueMeshes;
        std::map<f32,RenderingMesh*> _TranslucidMeshes;
        
        std::vector<RenderingMesh*> rmeshes = RenderingComponent::GetRenderingMeshes(Scene);
        
        for (std::vector<RenderingMesh*>::iterator k=rmeshes.begin();k!=rmeshes.end();k++)
        {
            if ((*k)->Material->IsTransparent() && sorting)
            {
                f32 index = CameraPos.distanceSQR((*k)->renderingComponent->GetOwner()->GetWorldPosition());
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
    
    void CubemapRenderer::RenderCubeMap(SceneGraph* Scene, const Matrix &AllSeeingEye, const f32 Near, const f32 Far)
    {
        
        InitRender();
        
        this->AllSeeingEye = AllSeeingEye;
        ProjectionMatrix = Matrix::PerspectiveMatrix(90.f, 1.0, Near, Far);
        
        // Universal Cache
        NearFarPlane = Vec2(Near, Far);
        CameraPosition = AllSeeingEye.GetTranslation();

        // Flags
        ViewMatrixInverseIsDirty = true;
        ProjectionMatrixInverseIsDirty = true;
        ViewProjectionMatrixIsDirty = true;
        
        // Group and Sort Meshes
        std::vector<RenderingMesh*> rmesh = GroupAndSortAssets(Scene,AllSeeingEye.GetTranslation());
        
        // Get Lights List
        std::vector<IComponent*> lcomps = ILightComponent::GetLightsOnScene(Scene);
        
        if (rmesh.size()>0)
        {
            
            // Prepare and Pack Lights to Send to Shaders
			std::vector<Matrix> _Lights;

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
					switch (((ILightComponent*)(*i))->GetLightType())
					{
						case LIGHT_TYPE::DIRECTIONAL:
						{
							DirectionalLight* d = (DirectionalLight*)(*i);

							// Directional Lights
							Vec4 color = d->GetLightColor();
							Vec3 position;
							Vec3 direction = (d->GetOwner()->GetWorldTransformation() * Vec4(d->GetLightDirection(), 0.f)).xyz().normalize();
							f32 attenuation = 1.f;
							Vec2 cones;
							int32 type = 1;

							Matrix directionalLight = Matrix();
							directionalLight.m[0] = color.x;         directionalLight.m[1] = color.y;             directionalLight.m[2] = color.z;             directionalLight.m[3] = color.w;
							directionalLight.m[4] = position.x;      directionalLight.m[5] = position.y;          directionalLight.m[6] = position.z;
							directionalLight.m[7] = direction.x;     directionalLight.m[8] = direction.y;         directionalLight.m[9] = direction.z;
							directionalLight.m[10] = attenuation;   //directionalLight.m[11] = attenuation.y;       directionalLight.m[12] = attenuation.z;
							directionalLight.m[13] = cones.x;         directionalLight.m[14] = cones.y;
							directionalLight.m[15] = (f32)type;

							Lights.push_back(directionalLight);
						}
						break;
						case LIGHT_TYPE::POINT:
						{
							PointLight* p = (PointLight*)(*i);

							// Point Lights
							Vec4 color = p->GetLightColor();
							Vec3 position = (p->GetOwner()->GetWorldPosition());
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
							pointLight.m[15] = (f32)type;

							Lights.push_back(pointLight);
						}
						break;
						case LIGHT_TYPE::SPOT:
						{
							SpotLight* s = (SpotLight*)(*i);

							// Spot Lights
							Vec4 color = s->GetLightColor();
							Vec3 position = (s->GetOwner()->GetWorldPosition());
							Vec3 direction = (s->GetOwner()->GetWorldTransformation() * Vec4(s->GetLightDirection(), 0.f)).xyz().normalize();
							f32 attenuation = s->GetLightRadius();
							Vec2 cones = Vec2(s->GetLightCosInnerCone(), s->GetLightCosOutterCone());
							int32 type = 3;

							Matrix spotLight = Matrix();
							spotLight.m[0] = color.x;        spotLight.m[1] = color.y;            spotLight.m[2] = color.z;            spotLight.m[3] = color.w;
							spotLight.m[4] = position.x;     spotLight.m[5] = position.y;         spotLight.m[6] = position.z;
							spotLight.m[7] = direction.x;    spotLight.m[8] = direction.y;        spotLight.m[9] = direction.z;
							spotLight.m[10] = attenuation;  //spotLight.m[11] = attenuation.y;      spotLight.m[12] = attenuation.z;
							spotLight.m[13] = cones.x;        spotLight.m[14] = cones.y;
							spotLight.m[15] = (f32)type;

							Lights.push_back(spotLight);
						}
						break;
					};
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
                ViewMatrix*=AllSeeingEye;
            
                // Frame Buffer Attachment
                fbo->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0,TextureType::CubemapPositive_X+i,environmentMap);

                // Set Viewport
                _SetViewPort(0,0, Width, Height);
            
                // Clear Screen
                ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
                ClearDepthBuffer();
                ClearScreen();

                // Draw Background
                DrawBackground();
                
                // Update Culling
                UpdateCulling(ProjectionMatrix*ViewMatrix);
                
                // Render Scene with Objects Material
                for (std::vector<RenderingMesh*>::iterator k=rmesh.begin();k!=rmesh.end();k++)
                {
					Lights.clear();
                    if ((*k)->renderingComponent->GetOwner()!=NULL)
                    {
                        // Culling Test
                        bool cullingTest = false;
                        switch((*k)->CullingGeometry)
                        {
                            case CullingGeometry::Box:
                                cullingTest = CullingBoxTest((*k),(*k)->renderingComponent->GetOwner());
                                break;
                            case CullingGeometry::Sphere:
                            default:
                                cullingTest = CullingSphereTest((*k),(*k)->renderingComponent->GetOwner());
                                break;
                        }
						if (!(*k)->renderingComponent->IsCullTesting()) cullingTest = true;
						if (cullingTest && (*k)->renderingComponent->IsActive() && (*k)->Active == true)
						{
							for (std::vector<Matrix>::iterator _l = _Lights.begin(); _l != _Lights.end(); _l++)
							{
								if ((*_l).m[13] == 1) Lights.push_back(*_l);
								else if ((*_l).m[13] == 2 || (*_l).m[13] == 3)
								{
									Vec3 _lPos = Vec3((*_l).m[4], (*_l).m[5], (*_l).m[6]);
									if ((_lPos.distance((*k)->renderingComponent->GetOwner()->GetWorldPosition()) - ((*k)->renderingComponent->GetOwner()->GetBoundingSphereRadiusWorldSpace())) < (*_l).m[10])
										Lights.push_back(*_l);
								}
							}
							NumberOfLights = Lights.size();
							RenderObject((*k), (*k)->renderingComponent->GetOwner(), (*k)->Material);
						}
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
