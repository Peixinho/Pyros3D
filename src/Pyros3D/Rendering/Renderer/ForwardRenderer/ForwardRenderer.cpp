//============================================================================
// Name        : ForwardRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Forward Renderer
//============================================================================

#include <Pyros3D/Rendering/Renderer/ForwardRenderer/ForwardRenderer.h>
#include <Pyros3D/Other/PyrosGL.h>
namespace p3d {
    
    namespace Sort {

        GameObject* _Camera;
        bool sortRenderingMeshes(const void* a, const void* b)
        {
            f32 a2 = _Camera->GetPosition().distanceSQR(((RenderingMesh*)a)->renderingComponent->GetOwner()->GetWorldPosition());
            f32 b2 = _Camera->GetPosition().distanceSQR(((RenderingMesh*)b)->renderingComponent->GetOwner()->GetWorldPosition());
            return ( a2<b2 );
        }
    }

    ForwardRenderer::ForwardRenderer(const uint32 Width, const uint32 Height) : IRenderer(Width,Height) 
    {
        echo("SUCCESS: Forward Renderer Created");
        
        ActivateCulling(CullingMode::FrustumCulling);

#if !defined(GLES2)
        shadowMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows);
        shadowMaterial->SetCullFace(CullFace::DoubleSided);
        shadowSkinnedMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::Skinning);
        shadowSkinnedMaterial->SetCullFace(CullFace::DoubleSided);
#endif

        // Default View Port Init Values
        viewPortStartX = viewPortStartY = 0;
        viewPortEndX = viewPortEndY = 0;
    }
    
    ForwardRenderer::~ForwardRenderer()
    {
        if (IsCulling)
        {
            delete culling;
        }

#if !defined(GLES2)
        delete shadowMaterial;
#endif

    }
    
    std::vector<RenderingMesh*> ForwardRenderer::GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 Tag)
    {
        
        // Sort and Group Objects From Scene
        std::vector<RenderingMesh*> _OpaqueMeshes;
        std::vector<RenderingMesh*> _TranslucidMeshes;
        
        // LOD
        if (lod)
        {
            std::vector<RenderingComponent*> comps(RenderingComponent::GetRenderingComponents(Scene));
            for (std::vector<RenderingComponent*>::iterator i=comps.begin();i!=comps.end();i++)
            {
                f32 distance = (Camera->GetWorldPosition().distanceSQR((*i)->GetOwner()->GetWorldPosition()+((*i)->GetBoundingSphereCenter()-(*i)->GetBoundingSphereRadius())*(*i)->GetOwner()->GetScale()));
                (*i)->UpdateLOD((*i)->GetLODByDistance(fabs(distance)));
            }
        }
		// Get Meshes
        std::vector<RenderingMesh*> rmeshes(RenderingComponent::GetRenderingMeshes(Scene));
		
		if (Tag!=0)
			for (std::vector<RenderingMesh*>::iterator k=rmeshes.begin();k!=rmeshes.end();)
				if (!(*k)->renderingComponent->GetOwner()->HaveTag(Tag))
					k = rmeshes.erase(k);
				else ++k;

        for (std::vector<RenderingMesh*>::iterator k=rmeshes.begin();k!=rmeshes.end();k++)
        {
            if ((*k)->Material->IsTransparent() && sorting)
            {
                _TranslucidMeshes.push_back((*k));
            }
            else _OpaqueMeshes.push_back((*k));
        }
        
        // sorting translucid
        Sort::_Camera = Camera;
        sort(_TranslucidMeshes.begin(), _TranslucidMeshes.end(), Sort::sortRenderingMeshes);

        // final list
        for (std::vector<RenderingMesh*>::iterator i=_TranslucidMeshes.begin();i!=_TranslucidMeshes.end();i++)
        {
            _OpaqueMeshes.push_back((*i));
        }
        
        return _OpaqueMeshes;
    }
	void ForwardRenderer::RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene) 
	{
		RenderSceneByTag(projection, Camera, Scene, 0);
    }
    void ForwardRenderer::RenderSceneByTag(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const std::string &Tag)
    {
		RenderSceneByTag(projection, Camera, Scene, MakeStringID(Tag));
	}
    void ForwardRenderer::RenderSceneByTag(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const uint32 Tag)
    {
        
        // Initialize Renderer
        InitRender();
        
        // Group and Sort Meshes
        std::vector<RenderingMesh*> rmesh = GroupAndSortAssets(Scene, Camera, Tag);
        
        // Get Lights List
        std::vector<IComponent*> lcomps = ILightComponent::GetLightsOnScene(Scene);
        
        // Save Time
        Timer = Scene->GetTime();
        
        if (rmesh.size()>0)
        {

            // Prepare and Pack Lights to Send to Shaders
            Lights.clear();

            if (lcomps.size()>0) 
            {

                // Keep user settings
                uint32 _bufferOptions = bufferOptions;
                uint32 _glBufferOptions = glBufferOptions;
                bool _depthWritting = depthWritting;
                bool _depthTesting = depthTesting;
                bool _clearDepthBuffer = clearDepthBuffer;

                ClearBufferBit(Buffer_Bit::Depth);
                EnableDepthTest();
                EnableDepthWritting();
                EnableClearDepthBuffer();

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

                ViewMatrix = Camera->GetWorldTransformation().Inverse();
                for (std::vector<IComponent*>::iterator i = lcomps.begin();i!=lcomps.end();i++)
                {
                    if (DirectionalLight* d = dynamic_cast<DirectionalLight*>((*i))) {

                        // Directional Lights
                        Vec4 color = d->GetLightColor();
                        Vec3 position;
                        Vec3 LDirection = d->GetLightDirection().normalize();
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

                        // Shadows
                        if (d->IsCastingShadows())
                        {
                            // Increase Number of Shadows
                            NumberOfDirectionalShadows++;

                            // Bind FBO
                            d->GetShadowFBO()->Bind();

                            // GPU
                            DepthTest();
                            DepthWrite();
                            ClearDepthBuffer();
                            ClearScreen();

                            // Enable Depth Bias
                            EnableDepthBias(Vec2(d->GetShadowBiasFactor(), d->GetShadowBiasUnits()));// enable polygon offset fill to combat "z-fighting"

                            ViewMatrix = d->GetLightViewMatrix();

                            // Get Lights Shadow Map Texture
                            for (uint32 i=0;i<d->GetNumberCascades();i++)
                            {
                                d->UpdateCascadeFrustumPoints(i,Camera->GetWorldPosition(),Camera->GetDirection());
                                ProjectionMatrix = d->GetLightProjection(i,rmesh);

                                // Set Viewport
                                _SetViewPort(((float)(i % 2) * d->GetShadowWidth()), ((i <= 1 ? 0.0f : 1.f) * d->GetShadowHeight()), d->GetShadowWidth(), d->GetShadowHeight());

                                // Update Culling
                                UpdateCulling(d->GetCascade(i).ortho.GetProjectionMatrix()*ViewMatrix);

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
                                                cullingTest = CullingBoxTest((*k),(*k)->renderingComponent->GetOwner());
                                                break;
                                            case CullingGeometry::Sphere:
                                            default:
                                                cullingTest = CullingSphereTest((*k),(*k)->renderingComponent->GetOwner());
                                                break;
                                        }
                                        if (!(*k)->Material->IsTransparent())
                                        {
                                            if ((*k)->renderingComponent->IsCastingShadows() && (*k)->renderingComponent->IsActive())
												RenderObject((*k), (*k)->renderingComponent->GetOwner(), ((*k)->SkinningBones.size()>0? shadowSkinnedMaterial : shadowMaterial));
                                        }
                                        else break;
                                    }
                                }

                                DirectionalShadowMatrix.push_back((Matrix::BIAS * (ProjectionMatrix * ViewMatrix)));

                            }

                            // Get Texture (only 1)
                            DirectionalShadowMapsTextures.push_back(d->GetShadowMapTexture());

                            // Set Shadow Far
                            Vec4 _ShadowFar;
                            if (d->GetNumberCascades()>0) _ShadowFar.x = d->GetCascade(0).Far;
                            if (d->GetNumberCascades()>1) _ShadowFar.y = d->GetCascade(1).Far;
                            if (d->GetNumberCascades()>2) _ShadowFar.z = d->GetCascade(2).Far;
                            if (d->GetNumberCascades()>3) _ShadowFar.w = d->GetCascade(3).Far;

                            Vec4 ShadowFar;
                            ShadowFar.x = 0.5f*(-_ShadowFar.x*projection.m.m[10]+projection.m.m[14])/_ShadowFar.x + 0.5f;
                            ShadowFar.y = 0.5f*(-_ShadowFar.y*projection.m.m[10]+projection.m.m[14])/_ShadowFar.y + 0.5f;
                            ShadowFar.z = 0.5f*(-_ShadowFar.z*projection.m.m[10]+projection.m.m[14])/_ShadowFar.z + 0.5f;
                            ShadowFar.w = 0.5f*(-_ShadowFar.w*projection.m.m[10]+projection.m.m[14])/_ShadowFar.w + 0.5f;
                            DirectionalShadowFar = ShadowFar;

                            // Disable Depth Bias
                            DisableDepthBias();

                            // Unbind FBO
                            d->GetShadowFBO()->UnBind();

                        }

                    } else if (PointLight* p = dynamic_cast<PointLight*>((*i))) {

                        ViewMatrix = Camera->GetWorldTransformation().Inverse();

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

                        // Shadows
                        if (p->IsCastingShadows())
                        {
                            // Increase Number of Shadows
                            NumberOfPointShadows++;

                            // Bind FBO
                            p->GetShadowFBO()->Bind();

                            ProjectionMatrix = p->GetLightProjection().GetProjectionMatrix();

                            // Get Lights Shadow Map Texture
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
                                ViewMatrix *= p->GetOwner()->GetWorldTransformation().Inverse();

                                // Update Culling
                                UpdateCulling(p->GetLightProjection().GetProjectionMatrix()*ViewMatrix);

                                // GPU Shadows
                                p->GetShadowFBO()->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment,TextureType::CubemapPositive_X+i,p->GetShadowMapTexture());

                                DepthTest();
                                DepthWrite();
                                ClearDepthBuffer();
                                ClearScreen();

                                // Enable Depth Bias
                                EnableDepthBias(Vec2(p->GetShadowBiasFactor(), p->GetShadowBiasUnits()));// enable polygon offset fill to combat "z-fighting"

                                // Set Viewport
                                _SetViewPort(0,0, p->GetShadowWidth(), p->GetShadowHeight());

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
                                                cullingTest = CullingBoxTest((*k),(*k)->renderingComponent->GetOwner());
                                                break;
                                            case CullingGeometry::Sphere:
                                            default:
                                                cullingTest = CullingSphereTest((*k),(*k)->renderingComponent->GetOwner());
                                                break;
                                        }
                                        if (cullingTest && !(*k)->Material->IsTransparent())
                                        {
                                            if ((*k)->renderingComponent->IsCastingShadows() && (*k)->renderingComponent->IsActive())
                                                RenderObject((*k), (*k)->renderingComponent->GetOwner(), ((*k)->SkinningBones.size()>0? shadowSkinnedMaterial : shadowMaterial));
                                        }
                                        else break;
                                    }
                                }
                            }

                            // Set Light Projection
                            PointShadowMatrix.push_back(p->GetLightProjection().GetProjectionMatrix());
                            // Set Light View Matrix
                            Matrix m;
                            m.Translate(p->GetOwner()->GetWorldPosition().negate());
                            PointShadowMatrix.push_back(m);

                            // Get Texture (only 1)
                            PointShadowMapsTextures.push_back(p->GetShadowMapTexture());

                            // Disable Depth Bias
                            DisableDepthBias();

                            // Unbind FBO
                            p->GetShadowFBO()->UnBind();

                        }

                    } else if (SpotLight* s = dynamic_cast<SpotLight*>((*i))) {

                        ViewMatrix = Camera->GetWorldTransformation().Inverse();

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

                        // Shadows
                        if (s->IsCastingShadows())
                        {

                            // Increase Number of Shadows
                            NumberOfSpotShadows++;

                            // Bind FBO
                            s->GetShadowFBO()->Bind();

                            // Get Light Projection
                            ProjectionMatrix = s->GetLightProjection().GetProjectionMatrix();

                            // Clean View Matrix
                            ViewMatrix.identity();

                            // Create Light View Matrix For Rendering the ShadowMap
                            ViewMatrix.LookAt(s->GetOwner()->GetWorldPosition(),(s->GetOwner()->GetWorldPosition()+s->GetLightDirection()));

                            // Update Culling
                            UpdateCulling(s->GetLightProjection().GetProjectionMatrix()*ViewMatrix);

                            DepthTest();
                            DepthWrite();
                            ClearDepthBuffer();
                            ClearScreen();

                            // Enable Depth Bias
                            EnableDepthBias(Vec2(s->GetShadowBiasFactor(), s->GetShadowBiasUnits()));// enable polygon offset fill to combat "z-fighting"

                            // Set Viewport
                            _SetViewPort(0,0, s->GetShadowWidth(), s->GetShadowHeight());

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
                                            cullingTest = CullingBoxTest((*k),(*k)->renderingComponent->GetOwner());
                                            break;
                                        case CullingGeometry::Sphere:
                                        default:
                                            cullingTest = CullingSphereTest((*k),(*k)->renderingComponent->GetOwner());
                                            break;
                                    }
                                    if (cullingTest && !(*k)->Material->IsTransparent())
                                    {
                                        if ((*k)->renderingComponent->IsCastingShadows() && (*k)->renderingComponent->IsActive())
                                                RenderObject((*k), (*k)->renderingComponent->GetOwner(), ((*k)->SkinningBones.size()>0? shadowSkinnedMaterial : shadowMaterial));
                                    }
                                    else break;
                                }
                            }

                            // Disable Depth Bias
                            DisableDepthBias();

                            // Unbind FBO
                            s->GetShadowFBO()->UnBind();

                            // Set Light Matrix
                            SpotShadowMatrix.push_back((Matrix::BIAS * (ProjectionMatrix * ViewMatrix)));

                            // Get Texture (only 1)
                            SpotShadowMapsTextures.push_back(s->GetShadowMapTexture());

                        }
                    }
                    // Universal Cache
                    ProjectionMatrix = projection.m;
                    NearFarPlane = Vec2(projection.Near, projection.Far);

                    // View Matrix and Position
                    ViewMatrix = Camera->GetWorldTransformation().Inverse();
                    CameraPosition = Camera->GetWorldPosition();
                }

                // Reset User Defined for Depth Buffer
                bufferOptions = _bufferOptions;
                glBufferOptions = _glBufferOptions;
                depthWritting = _depthWritting;
                depthTesting = _depthTesting;
                clearDepthBuffer = _clearDepthBuffer;

            }

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

            // Update Lights Position and Direction to ViewSpace
            NumberOfLights = Lights.size();

            // Set ViewPort
            if (viewPortEndX==0 || viewPortEndY==0) 
            { 
                viewPortEndX = Width;
                viewPortEndY = Height;
            }
            
            _SetViewPort(viewPortStartX,viewPortStartY,viewPortEndX,viewPortEndY);
            DepthTest();
            DepthWrite();
            ClearDepthBuffer();
            
            // Scissor Test
            if (scissorTest) 
            {
                glScissor(scissorTestX,scissorTestY,scissorTestWidth,scissorTestHeight);
                glEnable(GL_SCISSOR_TEST);
            }

            ClearScreen();

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
                            cullingTest = CullingSphereTest((*i),(*i)->renderingComponent->GetOwner());
                            break;
                    }
                    if (cullingTest && (*i)->renderingComponent->IsActive() && (*i)->Active == true)
                        RenderObject((*i),(*i)->renderingComponent->GetOwner(),(*i)->Material);
                }
            }
            // Disable Cull Face
            glDisable(GL_CULL_FACE);

            // Disable Scissor Test
            if (scissorTest)
            {
                glDisable(GL_SCISSOR_TEST);
            }

#if !defined(GLES2)
            // Set Default Polygon Mode
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

            // End Rendering
            EndRender();

            // Disable Blending
            DisableBlending();
        }
    }
    
};
