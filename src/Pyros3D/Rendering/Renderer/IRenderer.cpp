//============================================================================
// Name        : IRenderer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Renderer Interface
//============================================================================

#include "IRenderer.h"
#include <GL/glew.h>

namespace p3d {
    
    IRenderer::IRenderer() {}
    
    IRenderer::IRenderer(const uint32 &Width, const uint32 &Height)
    {
        // Set Background Default Color
        BackgroundColor = Vec4::ZERO;
        
        // Set Global Light Default Color
        GlobalLight = Vec4(0.2f,0.2f,0.2f,0.2f);
        
        // Save Dimensions
        this->Width = Width;
        this->Height = Height;
        
        // Depth Bias
        IsUsingDepthBias = false;
        
    }
    
    IRenderer::~IRenderer()
    {
        
    }
    
    void IRenderer::GroupAndSortAssets()
    {
        // Group and Sort
    }
    
    // Internal Function
    void IRenderer::RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene)
    {        
        
    }
    
    void IRenderer::RenderObject(RenderingMesh* rmesh, IMaterial* Material)
    {
        // model cache 
        ModelMatrix = rmesh->renderingComponent->GetOwner()->GetWorldTransformation();

        NormalMatrixIsDirty = true;
        ModelViewMatrixIsDirty = true;
        ModelViewProjectionMatrixIsDirty = true;
        ModelMatrixInverseIsDirty = true;
        ModelViewMatrixInverseIsDirty = true;
        ModelMatrixInverseTransposeIsDirty = true;
        
        // Render Mesh
        unsigned materialID = Material->GetShader();
        if (rmesh->ShadersAttributesCache.find(materialID)==rmesh->ShadersAttributesCache.end())
        {
            // Reset Attribute IDs
            for (std::vector<Renderables::AttributeBuffer*>::iterator i=rmesh->Geometry->AttributesBuffer.begin();i!=rmesh->Geometry->AttributesBuffer.end();i++)
            {
                std::vector<int> attribs;
                for (std::list<Renderables::VertexAttribute*>::iterator k=(*i)->Attributes.begin();k!=(*i)->Attributes.end();k++)
                {
                    attribs.push_back(-2);
                }
                rmesh->ShadersAttributesCache[materialID].push_back(attribs);
            }
            for (std::list<Uniform::Uniform>::iterator k = Material->GlobalUniforms.begin();k!=Material->GlobalUniforms.end();k++) 
            {
                rmesh->ShadersGlobalCache[materialID].push_back(-2);
            }
            for (std::list<Uniform::Uniform>::iterator k = Material->ModelUniforms.begin();k!=Material->ModelUniforms.end();k++) 
            {
                rmesh->ShadersModelCache[materialID].push_back(-2);
            }
            for (std::map<StringID,Uniform::Uniform>::iterator k = Material->UserUniforms.begin();k!=Material->UserUniforms.end();k++) 
            {
                rmesh->ShadersUserCache[materialID].push_back(-2);
            }
        }
        
        // Material Pre Render Function
        Material->PreRender();
        
        if (Material->IsCastingShadows())
        {
            ShadowMapsUnits.clear();
            for (std::vector<Texture>::iterator i = ShadowMapsTextures.begin(); i!=ShadowMapsTextures.end(); i++)
            {
                (*i).Bind();
                ShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
            }
        }
        // ################### SHADERS #########################

        
        // Run Shader Program
        if (LastProgramUsed==-1 || LastProgramUsed!=materialID)
        {
            
            LastProgramUsed = materialID;

            // Use Program
            glUseProgram(LastProgramUsed);

            // UNIFORMS
            // Send Global Uniforms
            SendGlobalUniforms(rmesh,Material);

            // Process Uniforms and other Material Stuff
            // on the Render function            

            // #################################################

            // Check if Material is DoubleSided
            if (Material->GetCullFace() != cullFace)
            {
                switch(Material->GetCullFace())
                {
                    case CullFace::FrontFace:
                        glEnable(GL_CULL_FACE);
                        glCullFace(GL_FRONT);
                        break;
                    case CullFace::DoubleSided:
                        glDisable(GL_CULL_FACE);
                        break;
                    case CullFace::BackFace:
                    default:
                        glEnable(GL_CULL_FACE);
                        glCullFace(GL_BACK);
                        break;
                }
                cullFace = Material->GetCullFace();
            }
            
            // Check if Material is WireFrame
            if (Material->IsWireFrame()==true)
            {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            } else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
        }
        
        // ################## Draw #########################

        if (InternalDrawType==-1 || InternalDrawType!=rmesh->GetDrawingType())
        // getting material drawing type
        switch(rmesh->GetDrawingType())
        {
            case DrawingType::Lines:
                DrawType = GL_LINES;
                break;
            case DrawingType::Polygons:
                DrawType = GL_POLYGON;
                break;
            case DrawingType::Points:
                DrawType = GL_POINTS;
                break;
            case DrawingType::Quads:
                DrawType = GL_QUADS;
                break;
            case DrawingType::Triangles_Fan:
                DrawType = GL_TRIANGLE_FAN;
                break;
            case DrawingType::Triangles_Strip:
                DrawType = GL_TRIANGLE_STRIP;
                break;
            case DrawingType::Triangles:
            default:
                DrawType = GL_TRIANGLES;
                break;
        }
        InternalDrawType = rmesh->GetDrawingType();
        
        // Material On Render Function
        Material->Render();

        // #################### ATTRIBUTES #####################

        // Check if custom Attributes exists
        if (rmesh->Geometry->AttributesBuffer.size()>0)
        {
            uint32 materialID = Material->GetShader();
            uint32 counterBuffers = 0;
            for (std::vector<Renderables::AttributeBuffer*>::iterator k=rmesh->Geometry->AttributesBuffer.begin();k!=rmesh->Geometry->AttributesBuffer.end();k++)
            {
                // Bind VAO
                glBindBuffer(GL_ARRAY_BUFFER, (*k)->Buffer->ID);

                // Get Struct Data
                if ((*k)->attributeSize==0)
                {
                    for (std::list<Renderables::VertexAttribute*>::iterator l=(*k)->Attributes.begin();l!=(*k)->Attributes.end();l++)
                    {
                        (*k)->attributeSize+=(*l)->byteSize;
                    }
                }
                
                // Counter
                unsigned counter = 0;
                for (std::list<Renderables::VertexAttribute*>::iterator l=(*k)->Attributes.begin();l!=(*k)->Attributes.end();l++)
                {
                    // Check if is not set
                    if (rmesh->ShadersAttributesCache[materialID][counterBuffers][counter]==-2)
                    {
                        // set VAO ID
                        rmesh->ShadersAttributesCache[materialID][counterBuffers][counter] = Shader::GetAttributeLocation(materialID,(*l)->Name);
                        
                    }
                    // If exists in shader
                    if (rmesh->ShadersAttributesCache[materialID][counterBuffers][counter]>=0)
                    {
                        glVertexAttribPointer(
                                                rmesh->ShadersAttributesCache[materialID][counterBuffers][counter],
                                                Buffer::Attribute::GetTypeCount((*l)->Type),
                                                Buffer::Attribute::GetType((*l)->Type),
                                                GL_FALSE,
                                                (*k)->attributeSize,
                                                BUFFER_OFFSET((*l)->Offset)
                                            );

                        // Enable Attribute
                        glEnableVertexAttribArray(rmesh->ShadersAttributesCache[materialID][counterBuffers][counter]);
                    }
                    counter++;
                }
                counterBuffers++;
            }
        }

        // Check if index buffer exists
        if (rmesh->Geometry->IndexBuffer!=NULL) 
        {
            // Bind Index Buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,rmesh->Geometry->IndexBuffer->ID);

            // Draw Rendering Component
            // Send Model Specific Uniforms
            SendModelUniforms(rmesh, Material);

            // Send User Specific Uniforms
            SendUserUniforms(rmesh,Material);
            
            // Draw
            glDrawElements(DrawType,rmesh->Geometry->IndexBuffer->GetGeometryData().size()/sizeof(int),GL_UNSIGNED_INT,BUFFER_OFFSET(0));                    
            
            // Unbind Index Buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }
        
        // Shadows
        if (Material->IsCastingShadows())
        {
            for (std::vector<Texture>::reverse_iterator i = ShadowMapsTextures.rbegin(); i!=ShadowMapsTextures.rend(); i++)
            {
                (*i).Unbind();
            }
        }
        // Material AfterRender Function
        Material->AfterRender();

        // Disable Attributes
        if (rmesh->Geometry->AttributesBuffer.size()>0)
        {
            unsigned counterBuffers = 0;
            for (std::vector<Renderables::AttributeBuffer*>::iterator k=rmesh->Geometry->AttributesBuffer.begin();k!=rmesh->Geometry->AttributesBuffer.end();k++)
            {
                unsigned counter = 0;
                for (std::list<Renderables::VertexAttribute*>::iterator l=(*k)->Attributes.begin();l!=(*k)->Attributes.end();l++)
                {                     
                    // If exists in shader
                    if (rmesh->ShadersAttributesCache[materialID][counterBuffers][counter]>=0)
                    {
                        glDisableVertexAttribArray(rmesh->ShadersAttributesCache[materialID][counterBuffers][counter]);
                    }
                    counter++;
                }
                counterBuffers++;
            }
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        
    }
    
    void IRenderer::ClearScreen(const uint32& Option)
    {
        uint32 Options = 0;
        if (Option | Buffer_Bit::Color) Options |= GL_COLOR_BUFFER_BIT;
        if (Option | Buffer_Bit::Depth) Options |= GL_DEPTH_BUFFER_BIT;
        if (Option | Buffer_Bit::Stencil) Options |= GL_STENCIL_BUFFER_BIT;
        
        glClear((GLuint)Options);
        glClearColor(BackgroundColor.x,BackgroundColor.y,BackgroundColor.z,BackgroundColor.w);
    }
    
    void IRenderer::EnableDepthTest()
    {
        // Enables Depth
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glClearDepth(1.f);
    }
    
    void IRenderer::SetGlobalLight(const Vec4& Light)
    {
        GlobalLight = Light;
    }
    
    void IRenderer::EnableDepthBias(const Vec2& Bias)
    {
        DepthBias = Bias;
        IsUsingDepthBias = true;
    }
    
    void IRenderer::DisableDepthBias()
    {
        IsUsingDepthBias = false;
    }
    
    void IRenderer::SetBackground(const Vec4& Color)
    {
        BackgroundColor = Color;
    }
    
    // Culling Methods
    void IRenderer::ActivateCulling(const unsigned& cullingType)
    {
        culling = new FrustumCulling();
        IsCulling = true;        
    }
    
    void IRenderer::DeactivateCulling()
    {
        IsCulling = false;
        delete culling;
    }
    
    bool IRenderer::CullingSphereTest(RenderingMesh* rmesh)
    {
        return culling->SphereInFrustum(rmesh->renderingComponent->GetOwner()->GetWorldPosition()+rmesh->Geometry->GetBoundingSphereCenter(),rmesh->Geometry->GetBoundingSphereRadius());
    }
    bool IRenderer::CullingBoxTest(RenderingMesh* rmesh)
    {
        // Get Bounding Values (min and max)
        Vec3 _min = rmesh->Geometry->GetBoundingMinValue();
        Vec3 _max = rmesh->Geometry->GetBoundingMaxValue();
        
        // Defining box vertex and Apply Transform
        Vec3 v[8];
        v[0] = rmesh->renderingComponent->GetOwner()->GetWorldTransformation() * _min;
        v[1] = rmesh->renderingComponent->GetOwner()->GetWorldTransformation() * Vec3(_min.x, _min.y, _max.z);
        v[2] = rmesh->renderingComponent->GetOwner()->GetWorldTransformation() * Vec3(_min.x, _max.y, _max.z);
        v[3] = rmesh->renderingComponent->GetOwner()->GetWorldTransformation() * _max;
        v[4] = rmesh->renderingComponent->GetOwner()->GetWorldTransformation() * Vec3(_min.x, _max.y, _min.z);
        v[5] = rmesh->renderingComponent->GetOwner()->GetWorldTransformation() * Vec3(_max.x, _min.y, _min.z);
        v[6] = rmesh->renderingComponent->GetOwner()->GetWorldTransformation() * Vec3(_max.x, _max.y, _min.z);
        v[7] = rmesh->renderingComponent->GetOwner()->GetWorldTransformation() * Vec3(_max.x, _min.y, _max.z);
        
        // Get new Min and Max
        Vec3 min = v[0];
        Vec3 max = v[0];
        for (uint32 i=1;i<8;i++)
        {
            if (v[i].x<min.x) min.x = v[i].x;
            if (v[i].y<min.y) min.y = v[i].y;
            if (v[i].z<min.z) min.z = v[i].z;
            if (v[i].x>max.x) max.x = v[i].x;
            if (v[i].y>max.y) max.y = v[i].y;
            if (v[i].z>max.z) max.z = v[i].z;
        }
        // Build new Box
        AABox aabb = AABox(min,max);
        
        // Return test
        return culling->ABoxInFrustum(aabb);
    }
    bool IRenderer::CullingPointTest(RenderingMesh* rmesh)
    {
        return culling->PointInFrustum(rmesh->renderingComponent->GetOwner()->GetWorldPosition());
    }    
    void IRenderer::UpdateCulling(const Matrix& view, const p3d::Projection& projection)
    {
        culling->Update(view,projection);
    }
    
    void IRenderer::SendGlobalUniforms(RenderingMesh* rmesh, IMaterial* Material)
    {
        // Send Global Uniforms
        unsigned counter = 0;
        for (std::list<Uniform::Uniform>::iterator k = Material->GlobalUniforms.begin();k!=Material->GlobalUniforms.end();k++) 
        {
            if (rmesh->ShadersGlobalCache[Material->GetShader()][counter] ==-2)
            {
                rmesh->ShadersGlobalCache[Material->GetShader()][counter]=Shader::GetUniformLocation(Material->GetShader(),(*k).Name);
                rmesh->ShadersGlobalCache[Material->GetShader()][counter] = rmesh->ShadersGlobalCache[Material->GetShader()][counter];
                
            }

            if (rmesh->ShadersGlobalCache[Material->GetShader()][counter]>=0)
            {
                switch ((*k).Usage)
                {
                    case Uniform::DataUsage::ViewMatrix:
                        Shader::SendUniform((*k),&ViewMatrix,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::ProjectionMatrix:
                        Shader::SendUniform((*k),&ProjectionMatrix,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::ViewProjectionMatrix:
                        if (ViewProjectionMatrixIsDirty==true)
                        {
                            ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
                            ViewProjectionMatrixIsDirty = false;
                        }
                        Shader::SendUniform((*k),&ViewProjectionMatrix,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::ViewMatrixInverse:
                        if (ViewMatrixInverseIsDirty==true)
                        {
                            ViewMatrixInverse = ViewMatrix.Inverse();
                            ViewMatrixInverseIsDirty = false;
                        }
                        Shader::SendUniform((*k),&ViewMatrixInverse,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::ProjectionMatrixInverse:
                        if (ProjectionMatrixInverseIsDirty==true)
                        {
                            ProjectionMatrixInverse = ProjectionMatrix.Inverse();
                            ProjectionMatrixInverseIsDirty = false;
                        }
                        Shader::SendUniform((*k),&ProjectionMatrixInverse,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::CameraPosition:
                        Shader::SendUniform((*k),&CameraPosition,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::Timer:
                        Shader::SendUniform((*k),&Timer,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::GlobalAmbientLight:
                        Shader::SendUniform((*k),&GlobalLight,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::Lights:
                        Shader::SendUniform((*k), &Lights[0], rmesh->ShadersGlobalCache[Material->GetShader()][counter], NumberOfLights);
                        break;
                    case Uniform::DataUsage::NumberOfLights:
                        Shader::SendUniform((*k),&NumberOfLights,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::NearFarPlane:
                        Shader::SendUniform((*k),&NearFarPlane,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::ShadowMap:
                        if (ShadowMapsUnits.size()>0)
                            Shader::SendUniform((*k),&ShadowMapsUnits[0],rmesh->ShadersGlobalCache[Material->GetShader()][counter],ShadowMapsUnits.size());
                        break;
                    case Uniform::DataUsage::ShadowMatrix:
                            Shader::SendUniform((*k),&ShadowMatrix[0],rmesh->ShadersGlobalCache[Material->GetShader()][counter],ShadowMatrix.size());
                        break;
                    case Uniform::DataUsage::ShadowFar:
                            Shader::SendUniform((*k),&ShadowFar,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::NumberOfShadows:
                        Shader::SendUniform((*k),&NumberOfShadows,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    default:
                        Shader::SendUniform((*k),rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                }
            }
            counter++;
        }
    }
    
    void IRenderer::SendUserUniforms(RenderingMesh* rmesh, IMaterial* Material)
    {
        // User Specific Uniforms
        unsigned counter = 0;
        for (std::map<StringID, Uniform::Uniform>::iterator k = Material->UserUniforms.begin();k!=Material->UserUniforms.end();k++) 
        {
            if (rmesh->ShadersUserCache[Material->GetShader()][counter]==-2)
            {
                rmesh->ShadersUserCache[Material->GetShader()][counter] = Shader::GetUniformLocation(Material->GetShader(),(*k).second.Name);
                rmesh->ShadersUserCache[Material->GetShader()][counter] = rmesh->ShadersUserCache[Material->GetShader()][counter];
            }
            
            if (rmesh->ShadersUserCache[Material->GetShader()][counter]>=0)
            {
                Shader::SendUniform((*k).second,rmesh->ShadersUserCache[Material->GetShader()][counter]);
            }
            counter++;
        }
    }
    
    void IRenderer::SendModelUniforms(RenderingMesh* rmesh, IMaterial* Material)
    {
        unsigned counter = 0;
        for (std::list<Uniform::Uniform>::iterator k = Material->ModelUniforms.begin();k!=Material->ModelUniforms.end();k++) 
        {
            if (rmesh->ShadersModelCache[Material->GetShader()][counter]==-2)
            {
                rmesh->ShadersModelCache[Material->GetShader()][counter] = Shader::GetUniformLocation(Material->GetShader(),(*k).Name);
                rmesh->ShadersModelCache[Material->GetShader()][counter] = rmesh->ShadersModelCache[Material->GetShader()][counter];
            }
            
            if (rmesh->ShadersModelCache[Material->GetShader()][counter]>=0)
            {
                switch ((*k).Usage)
                {
                case Uniform::DataUsage::ModelMatrix:
                    Shader::SendUniform((*k),&ModelMatrix,rmesh->ShadersModelCache[Material->GetShader()][counter]);
                    break;
                case Uniform::DataUsage::NormalMatrix:
                    if (NormalMatrixIsDirty==true)
                    {
                        NormalMatrix = (ViewMatrix*ModelMatrix);
                        NormalMatrixIsDirty = false;
                    }
                    Shader::SendUniform((*k),&NormalMatrix,rmesh->ShadersModelCache[Material->GetShader()][counter]);
                    break;
                case Uniform::DataUsage::ModelViewMatrix:
                    if (ModelViewMatrixIsDirty==true)
                    {
                        ModelViewMatrix = ViewMatrix*ModelMatrix;
                        ModelViewMatrixIsDirty = false;
                    }
                    Shader::SendUniform((*k),&ModelViewMatrix,rmesh->ShadersModelCache[Material->GetShader()][counter]);
                    break;                   
                case Uniform::DataUsage::ModelViewProjectionMatrix:
                    if (ModelViewProjectionMatrixIsDirty==true)
                    {
                        ModelViewProjectionMatrix = ProjectionMatrix*ViewMatrix*ModelMatrix;
                        ModelViewProjectionMatrixIsDirty = false;
                    }
                    Shader::SendUniform((*k),&ModelViewProjectionMatrix,rmesh->ShadersModelCache[Material->GetShader()][counter]);
                    break;
                case Uniform::DataUsage::ModelMatrixInverse:
                    if (ModelMatrixInverseIsDirty==true)
                    {
                        ModelMatrixInverse = ModelMatrix.Inverse();
                        ModelMatrixInverseIsDirty = false;
                    }
                    Shader::SendUniform((*k),&ModelMatrixInverse,rmesh->ShadersModelCache[Material->GetShader()][counter]);
                    break;
                case Uniform::DataUsage::ModelViewMatrixInverse:
                    if (ModelViewMatrixInverseIsDirty==true)
                    {
                        ModelViewMatrixInverse = (ViewMatrix*ModelMatrix).Inverse();
                        ModelViewMatrixInverseIsDirty = false;
                    }
                    Shader::SendUniform((*k),&ModelViewMatrixInverse,rmesh->ShadersModelCache[Material->GetShader()][counter]);
                    break;
                case Uniform::DataUsage::ModelMatrixInverseTranspose:
                    if (ModelMatrixInverseTransposeIsDirty==true)
                    {
                        ModelMatrixInverseTranspose = ModelMatrixInverse.Transpose();
                        ModelMatrixInverseTransposeIsDirty = false;
                    }
                    Shader::SendUniform((*k),&ModelMatrixInverseTranspose,rmesh->ShadersModelCache[Material->GetShader()][counter]);
                    break;
                case Uniform::DataUsage::SkinningBones:
//                    if (RenderingSubMeshComponent* sub = dynamic_cast<RenderingSubMeshComponent*> (rmesh))
//                    {
//                        Shader::SendUniform((*k),&sub->SkinningBones[0],sub->SkinningBones.size());
//                    }                        
                    break;
                }
            }
            counter++;
        }
    }
};