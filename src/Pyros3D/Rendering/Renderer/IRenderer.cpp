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
    
    // ViewPort Dimension
    uint32 IRenderer::_viewPortStartX = 0;
    uint32 IRenderer::_viewPortStartY = 0;
    uint32 IRenderer::_viewPortEndX = 0;
    uint32 IRenderer::_viewPortEndY = 0;
    
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
        
        // Blending
        Blending = false;
        
    }
    
    void IRenderer::Resize(const uint32& Width, const uint32& Height)
    {
        // Save Dimensions
        this->Width = Width;
        this->Height = Height;
    }
    
    void IRenderer::SetViewPort(const uint32& initX, const uint32& initY, const uint32& endX, const uint32& endY)
    {
        if (initX!=_viewPortStartX || initY!=_viewPortStartY || endX!=_viewPortEndX || endY != _viewPortEndY)
        {
            _viewPortStartX = initX;
            _viewPortStartY = initY;
            _viewPortEndX = endX;
            _viewPortEndY = endY;
            glViewport(initX,initY,endX,endY);
        }
    }
    
    IRenderer::~IRenderer()
    {
        
    }
    
    // Internal Function
    void IRenderer::RenderScene(const p3d::Projection &projection, GameObject* Camera, SceneGraph* Scene, bool clearScreen)
    {
        
    }
    
    void IRenderer::InitRender()
    {
        LastProgramUsed = -1;
        LastMaterialUsed = -1;
        LastMeshRendered = -1;
        InternalDrawType = -1;
        LastMaterialPTR = NULL;
        LastMeshRenderedPTR = NULL;
    }
    
    void IRenderer::EndRender()
    {
        if (LastMeshRenderedPTR!=NULL && LastMaterialPTR!=NULL)
        {
            // Unbind Index Buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            // Unbind Vertex Attributes
            UnbindMesh(LastMeshRenderedPTR,LastMaterialPTR);
            // Unbind Shadow Maps
            UnbindShadowMaps(LastMaterialPTR);
            // Material After Render
            LastMaterialPTR->AfterRender();
            // Unbind Shader Program
            glUseProgram(0);
            // Unset Pointers
            LastMaterialPTR = NULL;
            LastMeshRenderedPTR = NULL;
            LastProgramUsed = -1;
            LastMaterialUsed = -1;
            LastMeshRendered = -1;
            
            DisableBlending();
        }
    }
    
    void IRenderer::RenderObject(RenderingMesh* rmesh, IMaterial* Material)
    {
        // model cache
        ModelMatrix = rmesh->renderingComponent->GetOwner()->GetWorldTransformation() * rmesh->Pivot;
        
        NormalMatrixIsDirty = true;
        ModelViewMatrixIsDirty = true;
        ModelViewProjectionMatrixIsDirty = true;
        ModelMatrixInverseIsDirty = true;
        ModelViewMatrixInverseIsDirty = true;
        ModelMatrixInverseTransposeIsDirty = true;
        
        if ((LastMeshRenderedPTR!=rmesh || LastMaterialPTR!=Material) && LastProgramUsed!=-1)
        {
            // Unbind Index Buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            // Unbind Mesh
            UnbindMesh(LastMeshRenderedPTR,LastMaterialPTR);
            // Material Stuff After Render
            UnbindShadowMaps(LastMaterialPTR);
            // After Render
            LastMaterialPTR->AfterRender();
        }
        if (LastProgramUsed!=Material->GetShader()) glUseProgram(Material->GetShader());
        
        if (LastMeshRenderedPTR!=rmesh || LastMaterialPTR!=Material)
        {
            // Bind Mesh
            BindMesh(rmesh,Material);
            
            // Material Stuff Pre Render
            Material->PreRender();
            
            // Bind Shadow Maps
            BindShadowMaps(Material);
            
            // Send Global Uniforms
            SendGlobalUniforms(rmesh,Material);
            
            // Send Vertex Attributes
            SendAttributes(rmesh,Material);
            
            // Bind Index Buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,rmesh->Geometry->IndexBuffer->ID);
        }
        if (LastMaterialPTR!=Material)
        {
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
            (Material->IsWireFrame()?EnableWireFrame():DisableWireFrame());
            
            // Material Render Method
            Material->Render();
        }
        
        if (LastMeshRenderedPTR!=rmesh && (InternalDrawType==-1 || InternalDrawType!=rmesh->GetDrawingType()))
        {
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
        }
        
        // Send User Uniforms
        SendUserUniforms(rmesh, Material);
        
        // Send Model Specific Uniforms
        SendModelUniforms(rmesh, Material);
        
        // Enable / Disable Blending
        (rmesh->Material->IsTransparent()?EnableBlending():DisableBlending());
        
        // Draw
        glDrawElements(DrawType,rmesh->Geometry->IndexBuffer->GetGeometryData().size()/sizeof(int32),GL_UNSIGNED_INT,BUFFER_OFFSET(0));
        
        // Save Last Material and Mesh
        LastProgramUsed = Material->GetShader();
        LastMaterialPTR = Material;
        LastMaterialUsed = Material->GetInternalID();
        LastMeshRendered = rmesh->Geometry->GetInternalID();
        LastMeshRenderedPTR = rmesh;
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
        if (!IsUsingDepthBias)
        {
            IsUsingDepthBias = true;
            glEnable(GL_POLYGON_OFFSET_FILL);    // enable polygon offset fill to combat "z-fighting"
        }
        glPolygonOffset (Bias.x,Bias.y);
    }
    
    void IRenderer::DisableDepthBias()
    {
        if (IsUsingDepthBias)
        {
            IsUsingDepthBias = false;
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }
    
    void IRenderer::EnableBlending()
    {
        if (!Blending)
        {
            Blending = true;
            
            // Enable Blending
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }
    
    void IRenderer::DisableBlending()
    {
        if (Blending)
        {
            Blending = false;

            // Disables Blending
            glDisable(GL_BLEND);
        }
    }
    
    void IRenderer::EnableWireFrame()
    {
        if (!WireFrame)
        {
            WireFrame = true;
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
    }
     
    void IRenderer::DisableWireFrame()
    {
        if (WireFrame)
        {
            WireFrame = false;
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
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
    void IRenderer::UpdateCulling(const Matrix& ViewProjectionMatrix)
    {
        culling->Update(ViewProjectionMatrix);
    }
    
    void IRenderer::SendGlobalUniforms(RenderingMesh* rmesh, IMaterial* Material)
    {
        // Send Global Uniforms
        unsigned counter = 0;
        for (std::vector<Uniform::Uniform>::iterator k = Material->GlobalUniforms.begin();k!=Material->GlobalUniforms.end();k++)
        {
            if (rmesh->ShadersGlobalCache[Material->GetShader()][counter] ==-2)
            {
                rmesh->ShadersGlobalCache[Material->GetShader()][counter]=Shader::GetUniformLocation(Material->GetShader(),(*k).Name);
                rmesh->ShadersGlobalCache[Material->GetShader()][counter]=rmesh->ShadersGlobalCache[Material->GetShader()][counter];
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
                    case Uniform::DataUsage::DirectionalShadowMap:
                        if (DirectionalShadowMapsUnits.size()>0)
                            Shader::SendUniform((*k),&DirectionalShadowMapsUnits[0],rmesh->ShadersGlobalCache[Material->GetShader()][counter],DirectionalShadowMapsUnits.size());
                        break;
                    case Uniform::DataUsage::DirectionalShadowMatrix:
						if (DirectionalShadowMatrix.size()>0)
                            Shader::SendUniform((*k),&DirectionalShadowMatrix[0],rmesh->ShadersGlobalCache[Material->GetShader()][counter],DirectionalShadowMatrix.size());
                        break;
                    case Uniform::DataUsage::DirectionalShadowFar:
                        Shader::SendUniform((*k),&DirectionalShadowFar,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::NumberOfDirectionalShadows:
                        Shader::SendUniform((*k),&NumberOfDirectionalShadows,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::PointShadowMap:
                        if (PointShadowMapsUnits.size()>0)
                            Shader::SendUniform((*k),&PointShadowMapsUnits[0],rmesh->ShadersGlobalCache[Material->GetShader()][counter],PointShadowMapsUnits.size());
                        break;
                    case Uniform::DataUsage::PointShadowMatrix:
                        Shader::SendUniform((*k),&PointShadowMatrix[0],rmesh->ShadersGlobalCache[Material->GetShader()][counter],PointShadowMatrix.size());
                        break;
                    case Uniform::DataUsage::NumberOfPointShadows:
                        Shader::SendUniform((*k),&NumberOfPointShadows,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
                        break;
                    case Uniform::DataUsage::SpotShadowMap:
                        Shader::SendUniform((*k),&SpotShadowMapsUnits[0],rmesh->ShadersGlobalCache[Material->GetShader()][counter],SpotShadowMapsUnits.size());
                        break;
                    case Uniform::DataUsage::SpotShadowMatrix:
                        Shader::SendUniform((*k),&SpotShadowMatrix[0],rmesh->ShadersGlobalCache[Material->GetShader()][counter],SpotShadowMatrix.size());
                        break;
                    case Uniform::DataUsage::NumberOfSpotShadows:
                        Shader::SendUniform((*k),&NumberOfSpotShadows,rmesh->ShadersGlobalCache[Material->GetShader()][counter]);
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
        for (std::vector<Uniform::Uniform>::iterator k = Material->ModelUniforms.begin();k!=Material->ModelUniforms.end();k++)
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
    
    void IRenderer::BindMesh(RenderingMesh* rmesh, IMaterial* material)
    {
        if (rmesh->ShadersAttributesCache.find(material->GetShader())==rmesh->ShadersAttributesCache.end())
        {
            // Reset Attribute IDs
            for (std::vector<AttributeBuffer*>::iterator i=rmesh->Geometry->AttributesBuffer.begin();i!=rmesh->Geometry->AttributesBuffer.end();i++)
            {
                std::vector<int32> attribs;
                for (std::vector<VertexAttribute*>::iterator k=(*i)->Attributes.begin();k!=(*i)->Attributes.end();k++)
                {
                    attribs.push_back(Shader::GetAttributeLocation(material->GetShader(),(*k)->Name));
                }
                rmesh->ShadersAttributesCache[material->GetShader()].push_back(attribs);
            }
            for (std::vector<Uniform::Uniform>::iterator k = material->GlobalUniforms.begin();k!=material->GlobalUniforms.end();k++)
            {
                rmesh->ShadersGlobalCache[material->GetShader()].push_back(Shader::GetUniformLocation(material->GetShader(),(*k).Name));
            }
            for (std::vector<Uniform::Uniform>::iterator k = material->ModelUniforms.begin();k!=material->ModelUniforms.end();k++)
            {
                rmesh->ShadersModelCache[material->GetShader()].push_back(Shader::GetUniformLocation(material->GetShader(),(*k).Name));
            }
            for (std::map<StringID,Uniform::Uniform>::iterator k = material->UserUniforms.begin();k!=material->UserUniforms.end();k++)
            {
                rmesh->ShadersUserCache[material->GetShader()].push_back(Shader::GetUniformLocation(material->GetShader(),(*k).second.Name));
            }
        }
    }
    void IRenderer::UnbindMesh(RenderingMesh* rmesh, IMaterial* material)
    {
        // Disable Attributes
        if (rmesh->Geometry->AttributesBuffer.size()>0)
        {
            unsigned counterBuffers = 0;
            for (std::vector<AttributeBuffer*>::iterator k=rmesh->Geometry->AttributesBuffer.begin();k!=rmesh->Geometry->AttributesBuffer.end();k++)
            {
                unsigned counter = 0;
                for (std::vector<VertexAttribute*>::iterator l=(*k)->Attributes.begin();l!=(*k)->Attributes.end();l++)
                {
                    // If exists in shader
                    if (rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter]>=0)
                    {
                        glDisableVertexAttribArray(rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter]);
                    }
                    counter++;
                }
                counterBuffers++;
            }
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }
    
    void IRenderer::BindShadowMaps(IMaterial* material)
    {
        // Bind Shadows Textures
        if (material->IsCastingShadows())
        {
            DirectionalShadowMapsUnits.clear();
            for (std::vector<Texture*>::iterator i = DirectionalShadowMapsTextures.begin(); i!=DirectionalShadowMapsTextures.end(); i++)
            {
                (*i)->Bind();
                DirectionalShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
            }
            
            PointShadowMapsUnits.clear();
            for (std::vector<Texture*>::iterator i = PointShadowMapsTextures.begin(); i!=PointShadowMapsTextures.end(); i++)
            {
                (*i)->Bind();
                PointShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
            }
            
            SpotShadowMapsUnits.clear();
            for (std::vector<Texture*>::iterator i = SpotShadowMapsTextures.begin(); i!=SpotShadowMapsTextures.end(); i++)
            {
                (*i)->Bind();
                SpotShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
            }
        }
    }
    
    void IRenderer::UnbindShadowMaps(IMaterial* material)
    {
        // Unbind Shadows Textures
        if (material->IsCastingShadows())
        {
            // Spot Lights
            for (std::vector<Texture*>::reverse_iterator i = SpotShadowMapsTextures.rbegin(); i!=SpotShadowMapsTextures.rend(); i++)
            {
                (*i)->Unbind();
            }
            // Point Lights
            for (std::vector<Texture*>::reverse_iterator i = PointShadowMapsTextures.rbegin(); i!=PointShadowMapsTextures.rend(); i++)
            {
                (*i)->Unbind();
            }
            // Directional Lights
            for (std::vector<Texture*>::reverse_iterator i = DirectionalShadowMapsTextures.rbegin(); i!=DirectionalShadowMapsTextures.rend(); i++)
            {
                (*i)->Unbind();
            }
        }
    }
    
    void IRenderer::SendAttributes(RenderingMesh* rmesh, IMaterial* material)
    {
        // Check if custom Attributes exists
        if (rmesh->Geometry->AttributesBuffer.size()>0)
        {
            uint32 counterBuffers = 0;
            for (std::vector<AttributeBuffer*>::iterator k=rmesh->Geometry->AttributesBuffer.begin();k!=rmesh->Geometry->AttributesBuffer.end();k++)
            {
                // Bind VAO
                glBindBuffer(GL_ARRAY_BUFFER, (*k)->Buffer->ID);
                
                // Get Struct Data
                if ((*k)->attributeSize==0)
                {
                    for (std::vector<VertexAttribute*>::iterator l=(*k)->Attributes.begin();l!=(*k)->Attributes.end();l++)
                    {
                        (*k)->attributeSize+=(*l)->byteSize;
                    }
                }
                
                // Counter
                unsigned counter = 0;
                for (std::vector<VertexAttribute*>::iterator l=(*k)->Attributes.begin();l!=(*k)->Attributes.end();l++)
                {
                    // Check if is not set
                    if (rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter]==-2)
                    {
                        // set VAO ID
                        rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter] = Shader::GetAttributeLocation(material->GetShader(),(*l)->Name);
                        
                    }
                    // If exists in shader
                    if (rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter]>=0)
                    {
                        glVertexAttribPointer(
                                              rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter],
                                              Buffer::Attribute::GetTypeCount((*l)->Type),
                                              Buffer::Attribute::GetType((*l)->Type),
                                              GL_FALSE,
                                              (*k)->attributeSize,
                                              BUFFER_OFFSET((*l)->Offset)
                                              );
                        
                        // Enable Attribute
                        glEnableVertexAttribArray(rmesh->ShadersAttributesCache[material->GetShader()][counterBuffers][counter]);
                    }
                    counter++;
                }
                counterBuffers++;
            }
        }
    }
    
};