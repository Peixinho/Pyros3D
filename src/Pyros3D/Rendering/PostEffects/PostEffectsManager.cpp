//============================================================================
// Name        : PostEffectsManager.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Post Effects Manager
//============================================================================

#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {
    
    PostEffectsManager::PostEffectsManager(const uint32 width, const uint32 height)
    {
        
        // Save Dimensions
        Width = width;
        Height = height;
        
        // Create Quad
		CreateQuad();

		Color = new Texture();
        Color->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height);
        Color->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);
		
        Depth = new Texture();
        Depth->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height);
        Depth->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);

        // Initialize Internal FBO
        ExternalFBO = new FrameBuffer();
		ExternalFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, Depth);
		ExternalFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, Color);
        
        // Set FrameBuffers
        fbo1 = new FrameBuffer();
        fbo2 = new FrameBuffer();         

		Result1= new Texture();
        Result1->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height);
        Result1->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);

		Result2 = new Texture();
        Result2->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height);
        Result2->SetRepeat(TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge,TextureRepeat::ClampToEdge);

        // Init Frame Buffers
		fbo1->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, Result1);
		fbo2->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, Result2);
        
        // Set Flags
        usingFBO1 = usingFBO2 = false;
        activeFBO = NULL;        
        
        // Default Last RTT
        LastRTT = Color;
    }
    
    FrameBuffer* PostEffectsManager::GetExternalFrameBuffer()
    {
        return ExternalFBO;
    }

	void PostEffectsManager::CaptureFrame()
	{
		ExternalFBO->Bind();
	}
	void PostEffectsManager::EndCapture()
	{
		ExternalFBO->UnBind();
	}

	void PostEffectsManager::Resize(const uint32 width, const uint32 height)
    {
        // Save Dimensions
        Width = width;
        Height = height;
        
        // Resize External FBO
        ExternalFBO->Resize(Width, Height);
        
        // Resize FBOs
        fbo1->Resize(Width, Height);
        fbo2->Resize(Width, Height);

		// Resize Textures
		Color->Resize(Width, Height);
		Depth->Resize(Width, Height);
		LastRTT->Resize(Width, Height);
		Result1->Resize(Width, Height);
		Result2->Resize(Width, Height);
	
    }
    
    void PostEffectsManager::CreateQuad()
    {
        // Clear Geometry
        vertex.clear();
        texcoord.clear();

        float w2 = 1; float h2 = 1;
        
        // Set Quad Vertex
        Vec3 a = Vec3(-w2,-h2,0); Vec3 b = Vec3(w2,-h2,0); Vec3 c = Vec3(w2,h2,0); Vec3 d = Vec3(-w2,h2,0);
        vertex.push_back(a); texcoord.push_back(Vec2(0,0));
        vertex.push_back(b); texcoord.push_back(Vec2(1,0));
        vertex.push_back(c); texcoord.push_back(Vec2(1,1));

		vertex.push_back(c); texcoord.push_back(Vec2(1,1));
        vertex.push_back(d); texcoord.push_back(Vec2(0,1));
		vertex.push_back(a); texcoord.push_back(Vec2(0,0));
    } 
    
    void PostEffectsManager::ProcessPostEffects(Projection* projection)
    {
        // Set Counter
		uint32 counter = 1;

        // Save Near and Far Planes
        Vec2 NearFarPlane = Vec2(projection->Near, projection->Far);
        Vec2 ScreenDimensions = Vec2(Width,Height);

        // Run Through Effects
        for (std::vector<IEffect*>::iterator effect=effects.begin();effect!=effects.end();effect++)
        {
            if (counter == effects.size())
            {
                // Draw in Screen
                usingFBO1 = false;
                usingFBO2 = false;
                activeFBO = NULL;

				glViewport(0, 0, Width, Height);
                
            } else {
                // Draw to FBO
                
                // Select Available FBO
                if (usingFBO1) 
                {
                    usingFBO1 = false;
                    usingFBO2 = true;
                    activeFBO = fbo2;
                } else {
                    if (usingFBO2) 
                    {
                        usingFBO2 = false;
                    }
                    usingFBO1 = true;
                    activeFBO = fbo1;
                }
                
                // Set Custom Dimensions
                if ((*effect)->HaveCustomDimensions())
                {
					activeFBO->Resize((*effect)->GetWidth(), (*effect)->GetHeight());
					Texture* tex = activeFBO->GetAttachments()[0]->TexturePTR;
					tex->Resize((*effect)->GetWidth(), (*effect)->GetHeight());
					glViewport(0, 0, (*effect)->GetWidth(), (*effect)->GetHeight());
                    
                } else {
                    // Revert Default Dimensions
					activeFBO->Resize(Width, Height);
					Texture* tex = activeFBO->GetAttachments()[0]->TexturePTR;
					tex->Resize(Width, Height);
					glViewport(0, 0, Width, Height);
                }
                
                // Bind FBO
                activeFBO->Bind();

            }

            // Clear Screen
			GLCHECKER(glClearColor(0.f, 0.f, 0.f, 0.f));
            GLCHECKER(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

            // Start Shader Program
            GLCHECKER(glUseProgram((*effect)->shader->ShaderProgram()));

            // Bind MRT
            for (std::vector<RTT::Info>::iterator i = (*effect)->RTTOrder.begin();i != (*effect)->RTTOrder.end();i++)
            {
                switch((*i).Type)
                {
                    case RTT::Color:
                        Color->Bind();
                        break;
                    case RTT::Depth:
                        Depth->Bind();
                        break;
                    case RTT::LastRTT:
                        LastRTT->Bind();
                        break;
                    default:
                        (*i).texture->Bind();
                        break;
                }
            }

            // Send Uniforms
            for (std::map<uint32,__UniformPostProcess>::iterator i=(*effect)->Uniforms.begin();i!=(*effect)->Uniforms.end();i++)
            {
				if ((*i).second.handle==-2)
                {
					(*i).second.handle=Shader::GetUniformLocation((*effect)->shader->ShaderProgram(),(*i).second.uniform.Name);
                }
                if ((*i).second.handle!=-1)
                {
					switch((*i).second.uniform.Usage)
                    {
                        case PostEffects::NearFarPlane:
						{
							Shader::SendUniform((*i).second.uniform,&NearFarPlane,(*i).second.handle);
						}
                        break;
                        case PostEffects::ScreenDimensions:
                        {
                            Shader::SendUniform((*i).second.uniform,&ScreenDimensions,(*i).second.handle);
                        }
                        break;
                        default:
                        case PostEffects::Other:
						{
                            Shader::SendUniform((*i).second.uniform,(*i).second.handle);
						}
                        break;
                    }
                }
            }

            // Getting Attributes locations
            // Position
            if ((*effect)->positionHandle==-2)
            {
                (*effect)->positionHandle = Shader::GetAttributeLocation((*effect)->shader->ShaderProgram(),"aPosition");
            }
            // Texcoord
            if ((*effect)->texcoordHandle==-2)
            {
                (*effect)->texcoordHandle = Shader::GetAttributeLocation((*effect)->shader->ShaderProgram(),"aTexcoord");
            }

            // Send Attributes
            if ((*effect)->positionHandle>-1)
            {
                GLCHECKER(glEnableVertexAttribArray((*effect)->positionHandle));
                GLCHECKER(glVertexAttribPointer((*effect)->positionHandle, 3, GL_FLOAT, GL_FALSE, 0, &vertex[0]));
            }
            if ((*effect)->texcoordHandle>-1)
            {
                GLCHECKER(glEnableVertexAttribArray((*effect)->texcoordHandle));
                GLCHECKER(glVertexAttribPointer((*effect)->texcoordHandle, 2, GL_FLOAT, GL_FALSE, 0, &texcoord[0]));
            }

            // Draw Quad
			GLCHECKER(glDrawArrays(GL_TRIANGLES, 0, vertex.size()));

            // Disable Attributes
            if ((*effect)->texcoordHandle>-1)
            {
                GLCHECKER(glDisableVertexAttribArray((*effect)->texcoordHandle));
            }
            if ((*effect)->positionHandle>-1)
            {
                GLCHECKER(glDisableVertexAttribArray((*effect)->positionHandle));
            }

            // Unbind MRT      
            for (std::vector<RTT::Info>::reverse_iterator i = (*effect)->RTTOrder.rbegin();i != (*effect)->RTTOrder.rend();i++)
            {
                switch((*i).Type)
                {
                    case RTT::Color:
                        Color->Unbind();
                        break;
                    case RTT::Depth:
                        Depth->Unbind();
                        break;
                    case RTT::LastRTT:
                        LastRTT->Unbind();
                        break;
                    default:
                        (*i).texture->Unbind();
                        break;                        
                }
            }

            // Unbind FBO if is using and set the RTT
            if (usingFBO1|| usingFBO2)
            {
                activeFBO->UnBind();
                // Get RTT
				LastRTT = activeFBO->GetAttachments()[0]->TexturePTR;
            }

            // count loop
            counter++;

        }

		// Clear Flag
		usingFBO1 = usingFBO2 = false;

		// Disable Shader Program
		GLCHECKER(glUseProgram(0));       
    }
    
    PostEffectsManager::~PostEffectsManager() 
    {
        for (std::vector<IEffect*>::iterator i = effects.begin();i!=effects.end();i++)
        {
            delete (*i);
        }

        // Destroy FBOs
        delete fbo1;
        delete fbo2;
		delete ExternalFBO;

		// Destroy Textures
		delete Color;
		delete Depth;
		delete Result1;
		delete Result2;
    }
    
    void PostEffectsManager::AddEffect(IEffect* Effect)
    {
        // Add New Effect
        effects.push_back(Effect);
    }
    void PostEffectsManager::RemoveEffect(IEffect* Effect)
    {
        for (std::vector<IEffect*>::iterator i = effects.begin(); i!=effects.end();i++)
        {
            if ((*i)==Effect)
            {                
                effects.erase(i);
                break;
            }
        }        
    }
    
    const uint32 PostEffectsManager::GetNumberEffects() const
    {
        return effects.size();
    }

}