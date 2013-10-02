//============================================================================
// Name        : IRenderer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderer Interface
//============================================================================

#include "IRenderer.h"
#include "../Components/RenderingComponents/RenderingModelComponent/RenderingModelComponent.h"
#include "Culling/FrustumCulling/FrustumCulling.h"
#include "GL/glew.h"

namespace Fishy3D {

    IRenderer::IRenderer(const unsigned& width, const unsigned& height) 
    {
        // Save Dimensions
        Width = width;
        Height = height;
        
        // Defaults
        IsCulling = false;
        debugBoundingBoxes = debugNormals = false;
        FrameBufferDefined = false;
        sort = false;
        GlobalAmbientLight = vec4(.0,.0,.0,.0);
        BackgroundColor = vec4(0,0,0,0);
        
        // Cast Shadows
        castShadows = SuperSmartPointer<CastShadows> (new CastShadows());
        
        #if _DEBUG
        // Debug Material - Raw Creation using Generic Shader
        debugMaterial = SuperSmartPointer<IMaterial> (new GenericShaderMaterial(ShaderUsage::Color));
        GenericShaderMaterial* dbgMaterial = (GenericShaderMaterial*) debugMaterial.Get();
        dbgMaterial->SetColor(vec4(1,0,1,1));
        dbgMaterial->AddUniform(Uniform::Uniform("uProjectionMatrix",Uniform::DataUsage::ProjectionMatrix));
        dbgMaterial->AddUniform(Uniform::Uniform("uViewMatrix",Uniform::DataUsage::ViewMatrix));
        dbgMaterial->AddUniform(Uniform::Uniform("uModelMatrix",Uniform::DataUsage::ModelMatrix));
        #endif

    }

    void IRenderer::SetAmbientLight(const vec4& Color)
    {
        GlobalAmbientLight = Color;
    }
    
    void IRenderer::SetBackgroundColor(const vec4& Color)
    {
        BackgroundColor = Color;
    }
    
    void IRenderer::Resize(const unsigned& width, const unsigned& height)
    {
        // Save new Dimensions
        Width = width;
        Height = height;
    }
    
    void IRenderer::Sort(bool sort)
    {
        this->sort = sort;
    }
    
    void IRenderer::RenderByTag(SceneGraph* scene, GameObject* camera, Projection* projection, const std::string& Tag)
    {
        RenderByTag(scene, camera, projection, MakeStringID(Tag));
    }
    
    void IRenderer::SendGlobalUniforms(IMaterial* material)
    {
        // Send Global Uniforms
        for (std::list<Uniform::Uniform>::iterator k = material->GlobalUniforms.begin();k!=material->GlobalUniforms.end();k++) 
        {
            if ((*k).Handle==-2)
            {
                (*k).Handle=Shader::GetUniformLocation(material->GetShader(),(*k).Name);
            }

            if ((*k).Handle>=0)
            {
                switch ((*k).Usage)
                {
                    case Uniform::DataUsage::ViewMatrix:
                        Shader::SendUniform((*k),&ViewMatrix);
                        break;
                    case Uniform::DataUsage::ProjectionMatrix:
                        Shader::SendUniform((*k),&ProjectionMatrix);
                        break;
                    case Uniform::DataUsage::ViewProjectionMatrix:
                        if (ViewProjectionMatrixIsDirty==true)
                        {
                            ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
                            ViewProjectionMatrixIsDirty = false;
                        }
                        Shader::SendUniform((*k),&ViewProjectionMatrix);
                        break;
                    case Uniform::DataUsage::ViewMatrixInverse:
                        if (ViewMatrixInverseIsDirty==true)
                        {
                            ViewMatrixInverse = ViewMatrix.Inverse();
                            ViewMatrixInverseIsDirty = false;
                        }
                        Shader::SendUniform((*k),&ViewMatrixInverse);
                        break;
                    case Uniform::DataUsage::ProjectionMatrixInverse:
                        if (ProjectionMatrixInverseIsDirty==true)
                        {
                            ProjectionMatrixInverse = ProjectionMatrix.Inverse();
                            ProjectionMatrixInverseIsDirty = false;
                        }
                        Shader::SendUniform((*k),&ProjectionMatrixInverse);
                        break;
                    case Uniform::DataUsage::CameraPosition:
                        Shader::SendUniform((*k),&CameraPosition);
                        break;
                    case Uniform::DataUsage::Timer:
                        Shader::SendUniform((*k),&Timer);
                        break;
                    case Uniform::DataUsage::GlobalAmbientLight:
                        Shader::SendUniform((*k),&GlobalAmbientLight);
                        break;
                    case Uniform::DataUsage::Lights:
                        Shader::SendUniform((*k), &Lights[0], NumberOfLights);
                        break;
                    case Uniform::DataUsage::NumberOfLights:
                        Shader::SendUniform((*k),&NumberOfLights);
                        break;
                    case Uniform::DataUsage::NearFarPlane:
                        Shader::SendUniform((*k),&NearFarPlane);
                        break;
                    case Uniform::DataUsage::ShadowMap:
                        if (ShadowMapsUnits.size()>0)
                            Shader::SendUniform((*k),&ShadowMapsUnits[0],NumberOfShadows);
                        break;
                    case Uniform::DataUsage::ShadowMatrix:
                            Shader::SendUniform((*k),&ShadowMatrix[0],NumberOfShadows);
                        break;
                    case Uniform::DataUsage::ShadowFar:
                            Shader::SendUniform((*k),&ShadowFar);
                        break;
                    case Uniform::DataUsage::NumberOfShadows:
                        Shader::SendUniform((*k),&NumberOfShadows);
                        break;
                    default:
                        Shader::SendUniform((*k));
                        break;
                }
            }
        }
    }
    
    void IRenderer::SendUserUniforms(IMaterial* material)
    {
         // User Specific Uniforms
        for (std::map<StringID, Uniform::Uniform>::iterator k = material->UserUniforms.begin();k!=material->UserUniforms.end();k++) 
        {
            if ((*k).second.Handle==-2)
            {
                (*k).second.Handle=Shader::GetUniformLocation(material->GetShader(),(*k).second.Name);
            }

            if ((*k).second.Handle>=0)
            {
                Shader::SendUniform((*k).second);
            }
        }
    }
    
    void IRenderer::SendModelUniforms(IRenderingComponent* rcomp, IMaterial* material)
    {
        for (std::list<Uniform::Uniform>::iterator k = material->ModelUniforms.begin();k!=material->ModelUniforms.end();k++) 
        {
            if ((*k).Handle==-2)
            {
                (*k).Handle=Shader::GetUniformLocation(material->GetShader(),(*k).Name);
            }

            if ((*k).Handle>=0)
            {
                switch ((*k).Usage)
                {
                case Uniform::DataUsage::ModelMatrix:
                    Shader::SendUniform((*k),&ModelMatrix);
                    break;
                case Uniform::DataUsage::NormalMatrix:
                    if (NormalMatrixIsDirty==true)
                    {
                        NormalMatrix = ((ViewMatrix*ModelMatrix).Inverse().Transpose());
                        NormalMatrixIsDirty = false;
                    }
                    Shader::SendUniform((*k),&NormalMatrix);
                    break;
                case Uniform::DataUsage::ModelViewMatrix:
                    if (ModelViewMatrixIsDirty==true)
                    {
                        ModelViewMatrix = ViewMatrix*ModelMatrix;
                        ModelViewMatrixIsDirty = false;
                    }
                    Shader::SendUniform((*k),&ModelViewMatrix);
                    break;                   
                case Uniform::DataUsage::ModelViewProjectionMatrix:
                    if (ModelViewProjectionMatrixIsDirty==true)
                    {
                        ModelViewProjectionMatrix = ProjectionMatrix*ViewMatrix*ModelMatrix;
                        ModelViewProjectionMatrixIsDirty = false;
                    }
                    Shader::SendUniform((*k),&ModelViewProjectionMatrix);
                    break;
                case Uniform::DataUsage::ModelMatrixInverse:
                    if (ModelMatrixInverseIsDirty==true)
                    {
                        ModelMatrixInverse = ModelMatrix.Inverse();
                        ModelMatrixInverseIsDirty = false;
                    }
                    Shader::SendUniform((*k),&ModelMatrixInverse);
                    break;
                case Uniform::DataUsage::ModelViewMatrixInverse:
                    if (ModelViewMatrixInverseIsDirty==true)
                    {
                        ModelViewMatrixInverse = (ViewMatrix*ModelMatrix).Inverse();
                        ModelViewMatrixInverseIsDirty = false;
                    }
                    Shader::SendUniform((*k),&ModelViewMatrixInverse);
                    break;
                case Uniform::DataUsage::ModelMatrixInverseTranspose:
                    if (ModelMatrixInverseTransposeIsDirty==true)
                    {
                        ModelMatrixInverseTranspose = ModelMatrixInverse.Transpose();
                        ModelMatrixInverseTransposeIsDirty = false;
                    }
                    Shader::SendUniform((*k),&ModelMatrixInverseTranspose);
                    break;
                case Uniform::DataUsage::SkinningBones:
                    if (RenderingSubMeshComponent* sub = dynamic_cast<RenderingSubMeshComponent*> (rcomp))
                    {
                        Shader::SendUniform((*k),&sub->SkinningBones[0],sub->SkinningBones.size());
                    }                        
                    break;                    
                }
            }
        }
    }

	// Clear Opengl Context
	void IRenderer::ClearScreen()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(BackgroundColor.x,BackgroundColor.y,BackgroundColor.z,BackgroundColor.w);

		glEnable(GL_DEPTH_TEST);

		// enables depth writing
		glDepthMask(GL_TRUE);
		glClearDepth(1.f);
	}

    // Wireframe
    void IRenderer::StartRenderWireFrame()
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }
    void IRenderer::StopRenderWireFrame()
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    
    // Frame Buffer
    void IRenderer::SetFrameBuffer(SuperSmartPointer<FrameBuffer> fbo)
    {
        FBO = fbo.Get();
        FrameBufferDefined = true;
    }
    void IRenderer::DisableFrameBuffer()
    {
        FBO = NULL;
        FrameBufferDefined = false;
    }
           
    // Culling Methods
    void IRenderer::ActivateCulling(const unsigned& cullingType)
    {
        switch(cullingType)
        {
            case CullingMode::FrustumCulling:
                culling = SuperSmartPointer<Culling> (new FrustumCulling());                
                break;
        };
        cullMode = cullingType;
        IsCulling = true;        
    }
    
    void IRenderer::DeactivateCulling()
    {
        IsCulling = false;
        culling.Dispose();
    }
    
    bool IRenderer::CullingTest(IRenderingComponent* rcomp)
    {        
        
        vec3 _min, _max, min, max;
        _min = rcomp->GetOwner()->GetWorldMatrix()*rcomp->GetBoundingMin();
        _max = rcomp->GetOwner()->GetWorldMatrix()*rcomp->GetBoundingMax();
        min = _min;
        max = _max;
        if (_max.x<_min.x) min.x = _max.x;
        if (_max.y<_min.y) min.y = _max.y;
        if (_max.z<_min.z) min.z = _max.z;
        if (_min.x>_max.x) max.x = _min.x;
        if (_min.y>_max.y) max.y = _min.y;
        if (_min.z>_max.z) max.z = _min.z;
        AABox aabb = AABox(min,max);
        switch(cullMode) 
        {
            case CullingMode::FrustumCulling:
            {
                FrustumCulling* cull = (FrustumCulling*) culling.Get();
                return cull->ABoxInFrustum(aabb);
            }
            break;
        };
        return false;
    }
    
    void IRenderer::UpdateCulling(const Matrix& view, const Matrix& projection)
    {
        switch(cullMode) 
        {
            case CullingMode::FrustumCulling:
                FrustumCulling* cull = static_cast<FrustumCulling*> (culling.Get());
                cull->Update(view,projection);
                break;
        };
        
    }
    
    // Debug
    void IRenderer::StartDebugMode(const unsigned& DebugMode)
    {
        // Unset Debug Values
        debugBoundingBoxes = debugNormals = false;
        
        // Set Debug Values
        if (DebugMode & DebugDrawing::BoundingBox) { debugBoundingBoxes = true; }
        if (DebugMode & DebugDrawing::Normals) { debugNormals = true; }
        
    }
    void IRenderer::StopDebugMode()
    {
        // Unset Debug Values
        debugBoundingBoxes = debugNormals = false;
    }
    
    IRenderer::~IRenderer() 
    {
        // Destroy Internal Frame Buffer and Post Effects Manager
        internalFBO.Dispose();

        if (IsCulling == true)
            culling.Dispose();
        
        #ifdef _DEBUG
        debugMaterial.Dispose();
        #endif
    }
}