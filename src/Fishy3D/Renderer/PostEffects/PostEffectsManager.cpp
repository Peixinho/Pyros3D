//============================================================================
// Name        : PostEffectsManager.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Post Effects Manager
//============================================================================

#include "PostEffectsManager.h"
#include "Effects/NoEffect.h"
#include "GL/glew.h"

namespace Fishy3D {        
    
    PostEffectsManager::PostEffectsManager(const unsigned& width, const unsigned& height) : Width(width), Height(height)
    {
        
        // Save Dimensions
        Width = width;
        Height = height;
        
        // Create Quad
        UpdateQuad(Width, Height);
        ChangedDimensions = false;        
        
        // Set Projection
        proj.MakeOrthoRH(0,Width,Height,0,-1,1);
        
        // Initialize Internal FBO
        ExternalFBO = SuperSmartPointer<FrameBuffer> (new FrameBuffer());
        ExternalFBO->Init(Width,Height,FrameBufferTypes::Depth,FrameBufferAttachmentFormat::Color_Attachment_Floating_Point_16F);
        ExternalFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment_Floating_Point_16F);
        ExternalFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment_Floating_Point_16F);
        
        // Map FBO Textures from MRT
        Color = ExternalFBO->GetTexture(0);
        NormalDepth = ExternalFBO->GetTexture(1);
        Position = ExternalFBO->GetTexture(2);
        
        // Set FrameBuffers
        fbo1 = SuperSmartPointer<FrameBuffer> (new FrameBuffer());        
        fbo2 = SuperSmartPointer<FrameBuffer> (new FrameBuffer());         
        
        // Init Frame Buffers
        fbo1->Init(Width,Height,FrameBufferTypes::Depth,FrameBufferAttachmentFormat::Color_Attachment);
        fbo2->Init(Width,Height,FrameBufferTypes::Depth,FrameBufferAttachmentFormat::Color_Attachment);
        
        // Set Flags
        usingFBO1 = usingFBO2 = false;
        activeFBO = NULL;        
        
    }
    
    SuperSmartPointer<FrameBuffer> PostEffectsManager::GetExternalFrameBuffer()
    {
        return ExternalFBO;
    }

    void PostEffectsManager::Resize(const unsigned& width, const unsigned& height)
    {
        // Save Dimensions
        Width = width;
        Height = height;
        
        // Resize External FBO
        ExternalFBO->Resize(Width, Height);
        
        // Resize FBOs
        fbo1->Resize(Width, Height);
        fbo2->Resize(Width, Height);
        
        // Resize Ortho Matrix
        proj.MakeOrthoRH(0,Width,Height,0,-1,1);
        
        // Update Quad
        UpdateQuad(Width,Height);
        
    }
    
    void PostEffectsManager::UpdateQuad(const unsigned &width, const unsigned &height)
    {
        // Clear Geometry
        vertex.clear();
        texcoord.clear();
        
        float w2 = width/2; float h2 = height/2;   
        
        // Set Quad Vertex
        vec3 a = vec3(-w2,-h2,0); vec3 b = vec3(w2,-h2,0); vec3 c = vec3(w2,h2,0); vec3 d = vec3(-w2,h2,0);
        vertex.push_back(a); texcoord.push_back(vec2(0,0));
        vertex.push_back(b); texcoord.push_back(vec2(1,0));
        vertex.push_back(c); texcoord.push_back(vec2(1,1));
        vertex.push_back(d); texcoord.push_back(vec2(0,1));
    } 
    
    void PostEffectsManager::ProcessPostEffects(Projection* projection)
    {
        
        // Set Counter
        unsigned counter = 1;

        // Save Near and Far Planes
        vec2 NearFarPlane = vec2(projection->Near, projection->Far);
        
        // Run Through Effects
        for (std::vector<SuperSmartPointer <IEffect> >::iterator effect=effects.begin();effect!=effects.end();effect++)
        {

            if (counter == effects.size())
            {
                // Draw in Screen
                usingFBO1 = false;
                usingFBO2 = false;
                activeFBO = NULL; 
                
                if (ChangedDimensions==true)
                {
                    // Revert Dimensions to Default
                    UpdateQuad(Width,Height);
                    proj.MakeOrthoRH(0,Width,Height,0,-1,1);
                    // Reset Viewport
                    glViewport(0,0,Width,Height);
                    
                    ChangedDimensions = false;
                }
                
            } else {
                // Draw to FBO
                
                // Select Available FBO
                if (usingFBO1 == true) 
                {
                    usingFBO1 = false;
                    usingFBO2 = true;
                    activeFBO = fbo2;
                } else {
                    if (usingFBO2 == true) 
                    {
                        usingFBO2 = false;
                    }
                    usingFBO1 = true;
                    activeFBO = fbo1;
                }
                
                // Set Custom Dimensions
                if ((*effect)->HaveCustomDimensions() == true)
                {                    
                    UpdateQuad((*effect)->GetWidth(),(*effect)->GetHeight());
                    proj.MakeOrthoRH(0,(*effect)->GetWidth(),(*effect)->GetHeight(),0,-1,1);
                    activeFBO->Resize((*effect)->GetWidth(),(*effect)->GetHeight());
                    
                    ChangedDimensions = true;
                    
                } else {
                    // Revert Default Dimensions
                    if (ChangedDimensions == true)
                    {
                        UpdateQuad(Width,Height);
                        proj.MakeOrthoRH(0,Width,Height,0,-1,1);
                        activeFBO->Resize(Width,Height);
                        
                        ChangedDimensions = false;                        
                    }
                }
                
                // Bind FBO
                activeFBO->Bind();

            }

            // Clear Screen
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glClearColor(0.f,0.f,0.f,0.f);

            // Start Shader Program
            glUseProgram((*effect)->ShaderProgram());

            // Bind MRT
            for (std::vector<RTT::Info>::iterator i = (*effect)->RTTOrder.begin();i != (*effect)->RTTOrder.end();i++)
            {
                switch((*i).Type)
                {
                    case RTT::Color:
                        Color.Bind();
                        break;
                    case RTT::Normal_Depth:
                        NormalDepth.Bind();
                        break;
                    case RTT::Position:
                        Position.Bind();
                        break;
                    case RTT::LastRTT:
                        LastRTT.Bind();
                        break;
                    default:
                        (*i).texture.Bind();
                        break;                        
                }
            }

            // Send Uniforms
            for (std::vector<Uniform::Uniform>::iterator i=(*effect)->Uniforms.begin();i!=(*effect)->Uniforms.end();i++)
            {
                if ((*i).Handle==-2)
                {
                    (*i).Handle=Shader::GetUniformLocation((*effect)->ShaderProgram(),(*i).Name);
                }
                if ((*i).Handle!=-1)
                {                 
                    switch((*i).Usage)
                    {
                        case Uniform::PostEffects::ProjectionMatrix:
                        {
                            Matrix projection = proj.GetMatrix();
                            Shader::SendUniform((*i),&projection);
                        }
                            break;

                        case Uniform::PostEffects::NearFarPlane:
                            Shader::SendUniform((*i),&NearFarPlane);
                            break;
                        default:
                            Shader::SendUniform((*i));
                            break;
                    }                    
                }
            }
            // Getting Attributes locations
            // Position
            if ((*effect)->positionHandle==-2)
            {
                (*effect)->positionHandle = Shader::GetAttributeLocation((*effect)->ShaderProgram(),"aPosition");
            }
            // Texcoord
            if ((*effect)->texcoordHandle==-2)
            {
                (*effect)->texcoordHandle = Shader::GetAttributeLocation((*effect)->ShaderProgram(),"aTexcoord");
            }

            // Send Attributes
            if ((*effect)->positionHandle>-1)
            {
                glEnableVertexAttribArray((*effect)->positionHandle);        
                glVertexAttribPointer((*effect)->positionHandle, 3, GL_FLOAT, GL_FALSE, 0, &vertex[0]);
            }
            if ((*effect)->texcoordHandle>-1)
            {
                glEnableVertexAttribArray((*effect)->texcoordHandle);        
                glVertexAttribPointer((*effect)->texcoordHandle, 2, GL_FLOAT, GL_FALSE, 0, &texcoord[0]);
            }

                // Draw Quad
            glDrawArrays(GL_QUADS, 0, vertex.size());

            // Disable Attributes
            if ((*effect)->texcoordHandle>-1)
            {
                glDisableVertexAttribArray((*effect)->texcoordHandle);
            }
            if ((*effect)->positionHandle>-1)
            {
                glDisableVertexAttribArray((*effect)->positionHandle);
            }

            // Unbind MRT      
            for (std::vector<RTT::Info>::reverse_iterator i = (*effect)->RTTOrder.rbegin();i != (*effect)->RTTOrder.rend();i++)
            {
                switch((*i).Type)
                {
                    case RTT::Color:
                        Color.Unbind();
                        break;
                    case RTT::Normal_Depth:
                        NormalDepth.Unbind();
                        break;
                    case RTT::Position:
                        Position.Unbind();
                        break;
                    case RTT::LastRTT:
                        LastRTT.Unbind();
                        break;
                    default:
                        (*i).texture.Unbind();
                        break;                        
                }
            }

            // Unbind FBO if is using and set the RTT
            if (usingFBO1 == true || usingFBO2 == true)
            {
                activeFBO->UnBind();
                // Get RTT
                LastRTT = activeFBO->GetTexture(0);
            }

            // count loop
            counter++;

        }
       // Disable Shader Program
       glUseProgram(0);       
    }
    
    PostEffectsManager::~PostEffectsManager() 
    {
        for (std::vector<SuperSmartPointer <IEffect> >::iterator i = effects.begin();i!=effects.end();i++)
        {
            (*i).Dispose();
        }
        // Destroy FBOs
        fbo1.Dispose();
        fbo2.Dispose();
    }
    
    void PostEffectsManager::AddEffect(SuperSmartPointer<IEffect> Effect)
    {
        // Add New Effect
        effects.push_back(Effect);
    }
    void PostEffectsManager::RemoveEffect(SuperSmartPointer<IEffect> Effect)
    {
        for (std::vector<SuperSmartPointer <IEffect> >::iterator i = effects.begin(); i!=effects.end();i++)
        {
            if ((*i)==Effect)
            {                
                effects.erase(i);
                break;
            }
        }        
    }
    
    unsigned PostEffectsManager::GetNumberEffects()
    {
        return effects.size();
    }

}