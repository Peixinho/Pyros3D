//============================================================================
// Name        : DeferredRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Deferred Renderer
//============================================================================

#include "DeferredRenderer.h"
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {
    
    DeferredRenderer::DeferredRenderer(const uint32 Width, const uint32 Height, FrameBuffer* fbo) : IRenderer(Width,Height) 
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
        deferredMaterialDirectional = new CustomShaderMaterial("../../../../examples/DeferredRendering/assets/shaders/secondpassDirectional.glsl");
		deferredMaterialPoint = new CustomShaderMaterial("../../../../examples/DeferredRendering/assets/shaders/secondpassPoint.glsl");

        deferredMaterialDirectional->AddUniform(Uniforms::Uniform("tDepth", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniforms::Uniform("tDepth", Uniforms::DataType::Int, &texID));
        texID = 1;
		deferredMaterialDirectional->AddUniform(Uniforms::Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
        deferredMaterialPoint->AddUniform(Uniforms::Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
        texID = 2;
        deferredMaterialDirectional->AddUniform(Uniforms::Uniform("tSpecular", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniforms::Uniform("tSpecular", Uniforms::DataType::Int, &texID));
		texID = 3;
		deferredMaterialDirectional->AddUniform(Uniforms::Uniform("tNormal", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniforms::Uniform("tNormal", Uniforms::DataType::Int, &texID));

		deferredMaterialDirectional->AddUniform(Uniforms::Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		deferredMaterialDirectional->AddUniform(Uniforms::Uniform("uLightDirection", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
		deferredMaterialDirectional->AddUniform(Uniforms::Uniform("uLightColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		deferredMaterialDirectional->AddUniform(Uniforms::Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialDirectional->AddUniform(Uniforms::Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));
		deferredMaterialDirectional->AddUniform(Uniforms::Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		deferredMaterialDirectional->DisableDepthTest();
		deferredMaterialDirectional->DisableDepthWrite();

        deferredMaterialPoint->AddUniform(Uniforms::Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
        deferredMaterialPoint->AddUniform(Uniforms::Uniform("uLightPosition", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
        deferredMaterialPoint->AddUniform(Uniforms::Uniform("uLightRadius", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
        deferredMaterialPoint->AddUniform(Uniforms::Uniform("uLightColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
        deferredMaterialPoint->AddUniform(Uniforms::Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
        deferredMaterialPoint->AddUniform(Uniforms::Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
        deferredMaterialPoint->AddUniform(Uniforms::Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialPoint->AddUniform(Uniforms::Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialPoint->AddUniform(Uniforms::Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));
		deferredMaterialPoint->SetCullFace(CullFace::FrontFace);
		deferredMaterialPoint->DisableDepthTest();
		deferredMaterialPoint->DisableDepthWrite();
        
        // Light Volume
		quadHandle = new Plane(1, 1);
		directionalLight = new RenderingComponent(quadHandle);

        sphereHandle = new Sphere(1,4,4);
        pointLight = new RenderingComponent(sphereHandle);
    }
    
    void DeferredRenderer::Resize(const uint32 Width, const uint32 Height)
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
    
    std::vector<RenderingMesh*> DeferredRenderer::GroupAndSortAssets(SceneGraph* Scene, GameObject* Camera, const uint32 Tag)
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
            ClearDepthBuffer();
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
                            cullingTest = CullingSphereTest((*i), (*i)->renderingComponent->GetOwner());
                            break;
                    }
                    if (cullingTest && (*i)->renderingComponent->IsActive() && (*i)->Active == true)
                        RenderObject((*i),(*i)->renderingComponent->GetOwner(),(*i)->Material);
                }
            }
			
            // End Rendering
            EndRender();

            // Unbind FrameBuffer
            FBO->UnBind();

			// Initialize Rendering
			InitRender();
			
			ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
			ClearScreen();

			// Second Pass
			EnableBlending();
			BlendingEquation(BlendEq::Add);
			BlendingFunction(BlendFunc::One, BlendFunc::One);

            // Bind FBO Textures
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Depth_Attachment]->TexturePTR->Bind();
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Color_Attachment0]->TexturePTR->Bind();
            FBO->GetAttachments()[FrameBufferAttachmentFormat::Color_Attachment1]->TexturePTR->Bind();
			FBO->GetAttachments()[FrameBufferAttachmentFormat::Color_Attachment2]->TexturePTR->Bind();

            // Render Point Lights
            for (std::vector<IComponent*>::iterator i=lcomps.begin();i!=lcomps.end();i++)
            {

                if ((*i)->GetOwner()!=NULL)
                {
                    if (PointLight* p = dynamic_cast<PointLight*>((*i))) {
                        // Point Lights
							Vec3 pos = (ViewMatrix * Vec4(p->GetOwner()->GetWorldPosition(),1.0)).xyz();
                            Vec4 color = p->GetLightColor();
                            deferredMaterialPoint->SetUniformValue("uLightPosition", &pos);
                            deferredMaterialPoint->SetUniformValue("uLightRadius", p->GetLightRadius());
                            deferredMaterialPoint->SetUniformValue("uLightColor", &color);
							// Set Scale
							Matrix m; m.Scale(p->GetLightRadius(), p->GetLightRadius(), p->GetLightRadius());
                            pointLight->GetMeshes()[0]->Pivot = m;
                            RenderObject(pointLight->GetMeshes()[0],p->GetOwner(),deferredMaterialPoint);
                    }
                    else if (SpotLight* s = dynamic_cast<SpotLight*>((*i))) {
                        // Spot Lights


                    }
                    else if (DirectionalLight* d = dynamic_cast<DirectionalLight*>((*i))) {
                        // Directional Lights
						Vec3 dir = (ViewMatrix * Vec4(d->GetLightDirection(),0.0)).xyz();
						Vec4 color = d->GetLightColor();
						deferredMaterialDirectional->SetUniformValue("uLightDirection", &dir);
						deferredMaterialDirectional->SetUniformValue("uLightColor", &color);
						RenderObject(directionalLight->GetMeshes()[0], d->GetOwner(), deferredMaterialDirectional);
                    }
                }
            }
			
            // Disable Blending
            DisableBlending();

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
