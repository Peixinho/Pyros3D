//============================================================================
// Name        : ForwardRenderer.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Forward Renderer
//============================================================================

#include "ForwardRenderer.h"
#include "GL/glew.h"
#include "SFML/Graphics/RenderStates.hpp"

#include "../../Components/RenderingComponents/RenderingModelComponent/RenderingModelComponent.h"

namespace Fishy3D {

    ForwardRenderer::ForwardRenderer(const unsigned &width, const unsigned &height) : IRenderer(width, height) { }
    
    void ForwardRenderer::RenderScene(SceneGraph* scene, GameObject* camera, Projection* projection, bool clearScreen)
    {

        // sort objects
        if (sort == true)
        {
            scene->GetRenderingList()->Sort(CameraPosition);
        } else {
            scene->GetRenderingList()->SimpleSort();
        }        
        
        // Copy lists
        _OpaqueList = scene->GetRenderingList()->GetRenderingOpaqueList();
        _TranslucidList = scene->GetRenderingList()->GetRenderingTranslucidList();
        
        // Saves Scene
        this->scene = scene;
        
        // Saves Camera
        this->camera = camera;
        
        // Saves Projection
        this->projection = projection;
        
        // Set Culling Flag
        cullFace = -1;       
        
		// Cast Shadows
        castShadows->Cast(scene, _OpaqueList, camera);
		// Get Shadows maps and matrix
		scene->GetRenderingList()->ComputeShadows();
        
        // Render Scene
        Render(clearScreen);

        // Disable Opengl Culling
        cullFace = -1;
        glDisable(GL_CULL_FACE);
        
        // Clear Lists
        _OpaqueList.clear();
        _TranslucidList.clear();
        
    }
    
    void ForwardRenderer::RenderByTag(SceneGraph* scene, GameObject* camera, Projection* projection, const StringID& TagID, bool clearScreen)
    {        
        // sort objects
        if (sort == true)
        {
            scene->GetRenderingList()->Sort(CameraPosition);
        } else {
            scene->GetRenderingList()->SimpleSort();
        }
        
        if (scene->GetRenderingList()->GetRenderingOpaqueList().size()>0)
        {
            // Loop Through Rendering List Objects
            for (std::list <IRenderingComponent*>::const_iterator f=scene->GetRenderingList()->GetRenderingOpaqueList().begin();f!=scene->GetRenderingList()->GetRenderingOpaqueList().end();f++) 
            {
                // only one mesh
                if ((*f)->GetOwner()->CheckTag(TagID)==true)
                {
                    _OpaqueList.push_back((*f));
                }
            }            
        }

        if (scene->GetRenderingList()->GetRenderingTranslucidList().size()>0)
        {
            // Loop Through Rendering List Objects
            for (std::list <IRenderingComponent*>::const_iterator f=scene->GetRenderingList()->GetRenderingTranslucidList().begin();f!=scene->GetRenderingList()->GetRenderingTranslucidList().end();f++) 
            {
                // only one mesh
                if ((*f)->GetOwner()->CheckTag(TagID)==true)
                {
                    _TranslucidList.push_back((*f));
                }
            }
        }
        
        // Saves Scene
        this->scene = scene;
        
        // Saves Camera
        this->camera = camera;
        
        // Saves Projection
        this->projection = projection;

		// Cast Shadows
        castShadows->Cast(scene, _OpaqueList, camera);
		// Get Shadows maps and matrix
		scene->GetRenderingList()->ComputeShadows();

        // Render Scene
        Render(clearScreen);
        
        // Disable Opengl Culling
        glDisable(GL_CULL_FACE);

        // Clear Lists
        _OpaqueList.clear();
        _TranslucidList.clear();
        
    }
        
    void ForwardRenderer::Render(bool clearScreen) {
    
        // Enable Frame Buffer
        if (FrameBufferDefined==true)
        {
            FBO->Bind();
        } else {
            // If Is Not Using FBOs
            //Set Default ViewPort
           glViewport(0,0,Width,Height);
        }        
        
        if (clearScreen) ClearScreen();

        // Set Culling
        if (IsCulling == true)
            UpdateCulling(camera->GetWorldMatrix().Inverse(),projection->GetMatrix());
        
        // Last Program Used
        LastProgramUsed = -1;

        // Universal Cache
        ProjectionMatrix = projection->GetMatrix();
        NearFarPlane = vec2(projection->Near, projection->Far);
        
        // View Matrix and Position
        ViewMatrix = camera->GetWorldMatrix().Inverse();
        CameraPosition = camera->GetWorldPosition();

        // Flags
        ViewMatrixInverseIsDirty = true;
        ProjectionMatrixInverseIsDirty = true;
        ViewProjectionMatrixIsDirty = true;

        // Lights
        Lights = scene->GetRenderingList()->GetLightsPackaging();
        // Update Lights Position and Direction to ViewSpace
        for (unsigned i=0;i<Lights.size();i++)
        {
            vec3 pos = vec3(Lights[i].m[4],Lights[i].m[5],Lights[i].m[6]);
            pos = camera->GetWorldMatrix().Inverse() * pos;
            Lights[i].m[4] = pos.x;
            Lights[i].m[5] = pos.y;
            Lights[i].m[6] = pos.z;
            
            pos = vec3(Lights[i].m[7],Lights[i].m[8],Lights[i].m[9]);
            pos = camera->GetWorldMatrix().Transpose() * pos;
            Lights[i].m[7] = pos.x;
            Lights[i].m[8] = pos.y;
            Lights[i].m[9] = pos.z;
        }
        
        Timer = scene->GetTimer();
        NumberOfLights = (int)scene->GetRenderingList()->GetNumberOfLights();      
        
        // Shadows Casting
        ShadowMatrix = scene->GetRenderingList()->GetShadowMatrixBias();
        NumberOfShadows = scene->GetRenderingList()->GetNumberOfShadows();
        ShadowMapsTextures = scene->GetRenderingList()->GetShadowMaps();
        // Far planes of each cascade
        ShadowFar = scene->GetRenderingList()->GetShadowFar();
        // Transform to 0,1 coordinates
        ShadowFar.x = 0.5f*(-ShadowFar.x*ProjectionMatrix.m[10]+ProjectionMatrix.m[14])/ShadowFar.x + 0.5f;
        ShadowFar.y = 0.5f*(-ShadowFar.y*ProjectionMatrix.m[10]+ProjectionMatrix.m[14])/ShadowFar.y + 0.5f;
        ShadowFar.z = 0.5f*(-ShadowFar.z*ProjectionMatrix.m[10]+ProjectionMatrix.m[14])/ShadowFar.z + 0.5f;
        ShadowFar.w = 0.5f*(-ShadowFar.w*ProjectionMatrix.m[10]+ProjectionMatrix.m[14])/ShadowFar.w + 0.5f;
        
        if (_OpaqueList.size()>0 || _TranslucidList.size()>0)
        {
            // Opaque
            if (_OpaqueList.size()>0) 
            {

                // Loop Through Rendering List Objects
                for (std::list <IRenderingComponent*>::const_iterator f=_OpaqueList.begin();f!=_OpaqueList.end();f++) 
                {
                    // If IRendering Component is Active for Rendering
                    if ((*f)->IsActive()==true)
                    {

                        // model cache 
                        ModelMatrix = (*f)->GetOwner()->GetWorldMatrix();

                        NormalMatrixIsDirty = true;
                        ModelViewMatrixIsDirty = true;
                        ModelViewProjectionMatrixIsDirty = true;
                        ModelMatrixInverseIsDirty = true;
                        ModelViewMatrixInverseIsDirty = true;
                        ModelMatrixInverseTransposeIsDirty = true;

                        if (IsCulling==true)
                            if (CullingTest((*f))==false) continue;                        
                                ProcessRenderingComponent((*f));
                    }
                }
            }

            // Translucid
            if (_TranslucidList.size()>0)
            {

                // Enable Blending
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                // Loop Through Rendering List Objects
                for (std::list <IRenderingComponent*>::const_iterator f=_TranslucidList.begin();f!=_TranslucidList.end();f++) 
                {
                    // If IRendering Component is Active for Rendering
                    if ((*f)->IsActive()==true)
                    {
                        
                        // model cache
                        ModelMatrix = (*f)->GetOwner()->GetWorldMatrix();
                        
                        NormalMatrixIsDirty = true;
                        ModelViewMatrixIsDirty = true;
                        ModelViewProjectionMatrixIsDirty = true;
                        ModelMatrixInverseIsDirty = true;
                        ModelViewMatrixInverseIsDirty = true;
                        ModelMatrixInverseTransposeIsDirty = true;
                        
                        if (IsCulling==true)
                            if (CullingTest((*f))==false) continue;
                                ProcessRenderingComponent((*f));
                    }
            }
            // Disables Blending
            glDisable(GL_BLEND);
            }
            // Unset Shader Program
            glUseProgram(0);
        }
        
        // Disable Frame Buffer
        if (FrameBufferDefined==true)
        {
            FBO->UnBind();
        }
        
    }
    
    void ForwardRenderer::ProcessRenderingComponent(IRenderingComponent* rcomp)
    {
        
        // Material Pre Render Function
        rcomp->GetMaterial()->PreRender();        
        
        // Shadow Casting
        //if (ShadowMapsTextures.size()>0 && rcomp->GetMaterial()->IsCastingShadows() == true)
        //{
            ShadowMapsUnits.clear();
            for (std::vector<Texture>::iterator i = ShadowMapsTextures.begin(); i!=ShadowMapsTextures.end(); i++)
            {
                (*i).Bind();
                ShadowMapsUnits.push_back(Texture::GetLastBindedUnit());
            }
        //}
        
        // ################### SHADERS #########################

        
        // Run Shader Program
        if (LastProgramUsed==-1 || (unsigned long)LastProgramUsed!=rcomp->GetMaterial()->GetShader())
        {
            
            LastProgramUsed = rcomp->GetMaterial()->GetShader();

            // Use Program
            glUseProgram(LastProgramUsed);

            // UNIFORMS
            // Send Global Uniforms
            SendGlobalUniforms(rcomp->GetMaterial());

            // Process Uniforms and other Material Stuff
            // on the Render function            

            // #################################################

            // Check if Material is DoubleSided
            if (rcomp->GetMaterial()->GetCullFace() != cullFace)
            {
                switch(rcomp->GetMaterial()->GetCullFace())
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
                cullFace = rcomp->GetMaterial()->GetCullFace();
            }
            
            // Check if Material is WireFrame
            if (rcomp->GetMaterial()->IsWireFrame()==true)
            {
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            } else {
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }

            // ################## Draw #########################

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
        }
        
        // Material On Render Function
        rcomp->GetMaterial()->Render();

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
                    if ((*l)->ID==-2) 
                    {
                        // set VAO ID
                        (*l)->ID = Shader::GetAttributeLocation(rcomp->GetMaterial()->GetShader(),(*l)->Name);                            
                    }
                    // If exists in shader
                    if ((*l)->ID>=0)
                    {
                        glVertexAttribPointer(
                                (*l)->ID,
                                Buffer::Attribute::GetTypeCount((*l)->Type),
                                Buffer::Attribute::GetType((*l)->Type),
                                GL_FALSE,
                                (*k)->attributeSize,
                                BUFFER_OFFSET((*l)->Offset));

                        // Enable Attribute
                        glEnableVertexAttribArray((*l)->ID);
                    }
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
            SendModelUniforms(rcomp, rcomp->GetMaterial());

            // Send User Specific Uniforms
            SendUserUniforms(rcomp->GetMaterial());
            
            // Draw
            glDrawElements(drawType,rcomp->IndexBuffer->GetGeometryData().size()/sizeof(int),GL_UNSIGNED_INT,BUFFER_OFFSET(0));                    
            
            // Unbind Index Buffer
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }

        // Shadow Casting        
        //if (ShadowMapsTextures.size()>0  && rcomp->GetMaterial()->IsCastingShadows() == true)
        //{            
            for (std::vector<Texture>::reverse_iterator i = ShadowMapsTextures.rbegin(); i!=ShadowMapsTextures.rend(); i++)
            {
                (*i).Unbind();
            }
        //}
        
        // Material AfterRender Function
        rcomp->GetMaterial()->AfterRender();

        // Disable Attributes
        if (rcomp->Buffers.size()>0)
        {
            for (std::vector<SuperSmartPointer <AttributeBuffer> >::iterator k=rcomp->Buffers.begin();k!=rcomp->Buffers.end();k++)
            {
                for (std::list<SuperSmartPointer <VertexAttribute> >::iterator l=(*k)->Attributes.begin();l!=(*k)->Attributes.end();l++)
                {                     
                    // If exists in shader
                    if ((*l)->ID>=0)
                    {
                        glDisableVertexAttribArray((*l)->ID);
                    }
                }
            }
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        
        #if _DEBUG

        // Bind Program
        if (debugBoundingBoxes == true || debugNormals == true) 
        {
            glUseProgram(debugMaterial->GetShader());
            
            // Send Global Uniforms
            SendGlobalUniforms(debugMaterial);
            
            // Send Specific Uniforms
            SendUserUniforms(debugMaterial);
        
        
            if (debugBoundingBoxes == true)
            {

                // ##### Bounding Boxes #####

                // Bind VAO
                glBindBuffer(GL_ARRAY_BUFFER, rcomp->BoundingBuffer->Buffer->ID);            

                // Get Struct Data
                if (rcomp->BoundingBuffer->attributeSize==0)
                    for (std::list<SuperSmartPointer <VertexAttribute> >::iterator l=rcomp->BoundingBuffer->Attributes.begin();l!=rcomp->BoundingBuffer->Attributes.end();l++)
                    {
                        rcomp->BoundingBuffer->attributeSize+=(*l)->byteSize;
                    }

                // Bind VAOS
                for (std::list<SuperSmartPointer <VertexAttribute> >::iterator l=rcomp->BoundingBuffer->Attributes.begin();l!=rcomp->BoundingBuffer->Attributes.end();l++)
                {
                    // Check if is not set
                    if ((*l)->ID==-2) 
                    {
                        // set VAO ID
                        (*l)->ID = Shader::GetAttributeLocation(debugMaterial->GetShader(),(*l)->Name);                            
                    }
                    // If exists in shader
                    if ((*l)->ID>=0)
                    {
                        glVertexAttribPointer(
                                (*l)->ID,
                                Buffer::Attribute::GetTypeCount((*l)->Type),
                                Buffer::Attribute::GetType((*l)->Type),
                                GL_FALSE,
                                rcomp->BoundingBuffer->attributeSize,
                                BUFFER_OFFSET((*l)->Offset));

                        // Enable Attribute
                        glEnableVertexAttribArray((*l)->ID);
                    }
                }

                if (rcomp->IndexBoundingBuffer.Get()!=NULL) 
                {
                    // Bind Index               
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rcomp->IndexBoundingBuffer->ID);
                    SendModelUniforms(rcomp,debugMaterial);
                    glDrawElements(GL_LINES,rcomp->IndexBoundingBuffer->GetGeometryData().size()/sizeof(int),GL_UNSIGNED_INT,BUFFER_OFFSET(0));

                    // Unbind Index
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                }
                // Disable  Debug Attributes                
                for (std::list<SuperSmartPointer <VertexAttribute> >::iterator l=rcomp->BoundingBuffer->Attributes.begin();l!=rcomp->BoundingBuffer->Attributes.end();l++)
                {                     
                    // If exists in shader
                    if ((*l)->ID>=0)
                    {
                        glDisableVertexAttribArray((*l)->ID);
                    }
                }
                // Unbind VAOS
                glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
            if (debugNormals == true)
            {
                // ##### Normals #####

                // Bind VAO
                glBindBuffer(GL_ARRAY_BUFFER, rcomp->NormalsBuffer->Buffer->ID);            

                // Get Struct Data
                if (rcomp->NormalsBuffer->attributeSize==0)
                    for (std::list<SuperSmartPointer <VertexAttribute> >::iterator l=rcomp->NormalsBuffer->Attributes.begin();l!=rcomp->NormalsBuffer->Attributes.end();l++)
                    {
                        rcomp->NormalsBuffer->attributeSize+=(*l)->byteSize;
                    }

                // Bind VAOS
                for (std::list<SuperSmartPointer <VertexAttribute> >::iterator l=rcomp->NormalsBuffer->Attributes.begin();l!=rcomp->NormalsBuffer->Attributes.end();l++)
                {
                    // Check if is not set
                    if ((*l)->ID==-2) 
                    {
                        // set VAO ID
                        (*l)->ID = Shader::GetAttributeLocation(debugMaterial->GetShader(),(*l)->Name);                            
                    }
                    // If exists in shader
                    if ((*l)->ID>=0)
                    {
                        glVertexAttribPointer(
                                (*l)->ID,
                                Buffer::Attribute::GetTypeCount((*l)->Type),
                                Buffer::Attribute::GetType((*l)->Type),
                                GL_FALSE,
                                rcomp->NormalsBuffer->attributeSize,
                                BUFFER_OFFSET((*l)->Offset));

                        // Enable Attribute
                        glEnableVertexAttribArray((*l)->ID);
                    }
                }

                if (rcomp->IndexNormalsBuffer.Get()!=NULL) 
                {
                    // Bind Index
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, rcomp->IndexNormalsBuffer->ID);
                    
                    SendModelUniforms(rcomp,debugMaterial);                  
                    glDrawElements(GL_LINES,rcomp->IndexNormalsBuffer->GetGeometryData().size()/sizeof(int),GL_UNSIGNED_INT,BUFFER_OFFSET(0));

                    // Unbind Index
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                }
                // Disable  Debug Attributes                
                for (std::list<SuperSmartPointer <VertexAttribute> >::iterator l=rcomp->NormalsBuffer->Attributes.begin();l!=rcomp->NormalsBuffer->Attributes.end();l++)
                {                     
                    // If exists in shader
                    if ((*l)->ID>=0)
                    {
                        glDisableVertexAttribArray((*l)->ID);
                    }
                }
                // Unbind VAOS
                glBindBuffer(GL_ARRAY_BUFFER, 0);    
            }
        
        
            glUseProgram(LastProgramUsed);
        }
        #endif
    }
}