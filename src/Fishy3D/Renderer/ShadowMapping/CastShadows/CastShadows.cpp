//============================================================================
// Name        : CastShadows.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Class to Create the Shadow Maps
//============================================================================

#include "GL/glew.h"
#include "CastShadows.h"
#include "../../../Components/LightComponents/DirectionalLight/DirectionalLightComponent.h"

namespace Fishy3D {

    CastShadows::CastShadows()
    {
        material = SuperSmartPointer<IMaterial> (new CastShadowsMaterial());
    }   

    CastShadows::~CastShadows() {}
    
    void CastShadows::Cast(SceneGraph* scene, const std::list<IRenderingComponent*> &rcomps, GameObject* camera)
    {

        // Copy list
        RCompList = rcomps;

        for (std::map<StringID, ILightComponent*>::const_iterator light = scene->GetRenderingList()->GetLights().begin();light!=scene->GetRenderingList()->GetLights().end();light++)
        {
            // CASCADES
            DirectionalLightComponent* dl = (DirectionalLightComponent*)(*light).second;
			if (dl->IsUsingCascadeShadows())
			{
				vec3 LightDirection = dl->GetOwner()->GetWorldPosition().normalize()*-1.f;
				vec3 CameraDirection = camera->GetDirectionVector();
            
				// Light POV
				ViewMatrix.LookAtRH(vec3(0,0,0),LightDirection,vec3(0.f,0.f,-1.f));
				// Save View Matrix for Later Usage
				dl->GetCascadeShadowMapping()->ViewMatrix = ViewMatrix;
			
				for (unsigned i=0;i<dl->GetCascadeShadowMapping()->GetNumberOfSplits();i++)
				{
					// Update Frustum Points
					dl->GetCascadeShadowMapping()->f[i].UpdateFrustumPoints(camera->GetWorldPosition(),CameraDirection);
					// Create Ortho Projection
					ProjectionMatrix = dl->GetCascadeShadowMapping()->f[i].CreateCropMatrix(ViewMatrix, rcomps);
					// Bind FBO
					dl->GetCascadeShadowMapping()->f[i].fbo->Bind();
					// Render
					Render();
					// Unbind FBO
					dl->GetCascadeShadowMapping()->f[i].fbo->UnBind();
				}
			}
			// END CASCADES
		}
        // Clear List
        RCompList.clear();
        
    }
                
    void CastShadows::Render()
    {
          
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        // enables depth writing
        glDepthMask(GL_TRUE);
        glClearDepth(1.f);
        
        if (RCompList.size()>0) 
        {
            // Disable Face Culling
            glDisable(GL_CULL_FACE);
            
            // Use Shader Program
            glUseProgram(material->GetShader());
            
            // Bind Texture
            material->PreRender();
            
            // Send Global Uniforms
            SendGlobalUniforms(material);

            // Loop Through Rendering List Objects
            for (std::list <IRenderingComponent*>::iterator f=RCompList.begin();f!=RCompList.end();f++) 
            {
                // If IRendering Component is Active for Rendering
                if ((*f)->IsActive()==true)
                {
                    
                    // Model Cache 
                    ModelMatrix = (*f)->GetOwner()->GetWorldMatrix();
                    
                    // Process Geometry
                    ProcessRenderingComponent((*f));
                    
                }
            }
           
            // Unbind Texture
            material->AfterRender();
            
            // Unset Shader Program
            glUseProgram(0);
            
        }
        
    }
    
    void CastShadows::ProcessRenderingComponent(IRenderingComponent* rcomp)
    {

        // getting material drawing type
        switch(rcomp->GetDrawingType())
        {
            case DrawingType::Lines:
                drawType = GL_LINES;
                break;
            case DrawingType::Polygons:
                drawType = GL_POLYGON;
                break;
            case DrawingType::Points:
                drawType = GL_POINTS;
                break;
            case DrawingType::Quads:
                drawType = GL_QUADS;
                break;
            case DrawingType::Triangles_Fan:
                drawType = GL_TRIANGLE_FAN;
                break;
            case DrawingType::Triangles_Strip:
                drawType = GL_TRIANGLE_STRIP;
                break;
            case DrawingType::Triangles:
            default:
                drawType = GL_TRIANGLES;
                break;
        }        

        // #################### ATTRIBUTES #####################

        // Check if custom Attributes exists
        if (rcomp->Buffers.size()>0)
        {

            for (std::vector<SuperSmartPointer <AttributeBuffer> >::iterator k=rcomp->Buffers.begin();k!=rcomp->Buffers.end();k++)
            {	
                // Bind VAO
                glBindBuffer(GL_ARRAY_BUFFER, (*k)->Buffer->ID);

                // Get Struct Data
                if ((*k)->attributeSize==0)
                {
                    for (std::list<SuperSmartPointer <VertexAttribute> >::iterator l=(*k)->Attributes.begin();l!=(*k)->Attributes.end();l++)
                    {
                        (*k)->attributeSize+=(*l)->byteSize;
                    }
                }                
                for (std::list<SuperSmartPointer <VertexAttribute> >::iterator l=(*k)->Attributes.begin();l!=(*k)->Attributes.end();l++)
                {
                    // Check if is not set
                    if ((*l)->AuxID==-2)
                    {
                        // set VAO ID
                        (*l)->AuxID = Shader::GetAttributeLocation(material->GetShader(),(*l)->Name);                            
                    }
                    // If exists in shader
                    if ((*l)->AuxID>=0)
                    {
                        glVertexAttribPointer(
                                (*l)->AuxID,
                                Buffer::Attribute::GetTypeCount((*l)->Type),
                                Buffer::Attribute::GetType((*l)->Type),
                                GL_FALSE,
                                (*k)->attributeSize,
                                BUFFER_OFFSET((*l)->Offset));

                        // Enable Attribute
                        glEnableVertexAttribArray((*l)->AuxID);
                    }
                }
            }
        

            // Check if index buffer exists
            if (rcomp->IndexBuffer.Get()!=NULL) 
            {
                // Bind Index Buffer
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,rcomp->IndexBuffer->ID);

                // Draw Rendering Component
                // Send Model Specific Uniforms
                SendModelUniforms(rcomp, material);
                // Send User Uniforms
                SendUserUniforms(material);
				
                glDrawElements(drawType,rcomp->IndexBuffer->GetGeometryData().size()/sizeof(int),GL_UNSIGNED_INT,BUFFER_OFFSET(0));                    

                // Unbind Index Buffer
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
        }

        // Disable Attributes
        if (rcomp->Buffers.size()>0)
        {
            for (std::vector<SuperSmartPointer <AttributeBuffer> >::iterator k=rcomp->Buffers.begin();k!=rcomp->Buffers.end();k++)
            {
                for (std::list<SuperSmartPointer <VertexAttribute> >::iterator l=(*k)->Attributes.begin();l!=(*k)->Attributes.end();l++)
                {                     
                    // If exists in shader
                    if ((*l)->AuxID>=0)
                    {
                        glDisableVertexAttribArray((*l)->AuxID);
//                        (*l)->AuxID = -2; // Clears AuxID for Other Usage
                    }
                }
            }
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        
    }      

    void CastShadows::SendGlobalUniforms(IMaterial* material)
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
                    case Uniform::DataUsage::NearFarPlane:
                        Shader::SendUniform((*k),&NearFarPlane);
                        break;
                    default:
                        Shader::SendUniform((*k));
                        break;
                }
            }
        }
    }
    
    void CastShadows::SendUserUniforms(IMaterial* material)
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
    
    void CastShadows::SendModelUniforms(IRenderingComponent* rcomp, IMaterial* material)
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
                }
            }
        }
    }
    
    // Specific for Clones
    void CastShadows::SendModelUniforms(IRenderingComponent* rcomp, GameObject* Owner, IMaterial* material)
    {
         ModelMatrix = Owner->GetWorldMatrix();
         
        // Send Model Specific Uniforms
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
                }
            }
        }
    }    
    
    // Culling Methods
    void CastShadows::ActivateCulling(const unsigned& cullingType)
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
    
    void CastShadows::DeactivateCulling()
    {
        IsCulling = false;
        culling.Dispose();
    }
    
    bool CastShadows::CullingTest(IRenderingComponent* rcomp)
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
    }
    
    void CastShadows::UpdateCulling(const Matrix& view, const Matrix& projection)
    {
        switch(cullMode) 
        {
            case CullingMode::FrustumCulling:
                FrustumCulling* cull = static_cast<FrustumCulling*> (culling.Get());
                cull->Update(view,projection);
                break;
        };
        
    }    
    
}