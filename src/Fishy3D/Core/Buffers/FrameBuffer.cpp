//============================================================================
// Name        : FrameBuffer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : FrameBuffer
//============================================================================

#include "FrameBuffer.h"
#include "SFML/Window/VideoMode.hpp"
#include "GL/glew.h"

namespace Fishy3D {

    FrameBuffer::FrameBuffer() 
    {
        FBOInitialized = false;
    }

    FrameBuffer::~FrameBuffer() 
    {
        // destroy fbo and rbo
        glDeleteFramebuffers(1, (GLuint*)&fbo);
        
        if (useRenderBuffer==true)
            glDeleteRenderbuffers(1, (GLuint*)&rbo);
        
        for (int i=0;i<attachments.size();i++) 
            attachments[i].texture.DeleteTexture();
        
        // flag FBO Stoped
        FBOInitialized = false;
    }
    
    void FrameBuffer::Init(const unsigned int& width, const unsigned int& height, const unsigned int& frameBufferType, const unsigned int& internalAttatchmentFormat, bool mipmapping)
    {                
        
        // Save Dimensions
        Width = width;
        Height = height;        
        
        if (FBOInitialized==true)
        {
            // destroy fbo and rbo
            glDeleteFramebuffers(1, (GLuint*)&fbo);

            if (useRenderBuffer==true)
                glDeleteRenderbuffers(1, (GLuint*)&rbo);

            for (int i=0;i<attachments.size();i++) 
                attachments[i].texture.DeleteTexture();
        
            // flag FBO Stoped
            FBOInitialized = false;
        }
        
        // flag FBO Initialized
        FBOInitialized = true;

        // Save Type
        this->type = frameBufferType;

        // Get Internal Format
        switch(frameBufferType)
        {
            case FrameBufferTypes::Color:
                framebufferFormat = GL_RGBA8;
                break;                
            case FrameBufferTypes::Depth:
                framebufferFormat = GL_DEPTH_COMPONENT32;
                useRenderBuffer = true;
                break;               
            case FrameBufferTypes::Stencil:
                framebufferFormat = GL_STENCIL_INDEX;
                useRenderBuffer = true;
                break;
        };        
        
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        
        // create RBO
        if (useRenderBuffer==true)
        {
            glGenRenderbuffers(1, &rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);                        
            glRenderbufferStorage(GL_RENDERBUFFER, framebufferFormat, Width, Height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
        }
        
        AddAttach(internalAttatchmentFormat, mipmapping);

        #if _DEBUG
        switch ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) )
        {
            case GL_FRAMEBUFFER_COMPLETE:
            {
                std::cout << "FBO: The framebuffer is complete and valid for rendering." << std::endl;
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            {
                std::cout << "FBO: One or more attachment points are not framebuffer attachment complete. This could mean theres no texture attached or the format isnt renderable. For color textures this means the base format must be RGB or RGBA and for depth textures it must be a DEPTH_COMPONENT format. Other causes of this error are that the width or height is zero or the z-offset is out of range in case of render to volume." << std::endl;
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            {
                std::cout << "FBO: There are no attachments." << std::endl;
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            {
                std::cout << "FBO: Attachments are of different size. All attachments must have the same width and height." << std::endl;
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            {
                std::cout << "FBO: The color attachments have different format. All color attachments must have the same format." << std::endl;
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            {
                std::cout << "FBO: An attachment point referenced by GL.DrawBuffers() doesnt have an attachment." << std::endl;
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            {
                std::cout << "FBO: The attachment point referenced by GL.ReadBuffers() doesnt have an attachment." << std::endl;
                break;
            }
            case GL_FRAMEBUFFER_UNSUPPORTED:
            {
                std::cout << "FBO: This particular FBO configuration is not supported by the implementation." << std::endl;
                break;
            }
            default:
            {
                std::cout << "FBO: Status unknown. (yes, this is really bad.)" << std::endl;
                break;
            }
        }
        #endif

        // unbind FBO and RBO  
        if (useRenderBuffer==true)
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
 
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
    }
    
    void FrameBuffer::AddAttach(const unsigned int& internalAttatchmentFormat, bool mipmapping)
    {

        if (FBOInitialized==true)
        {
            // Temporary Texture
            Attachment attach;                        
            
            // Get Attatchment Format
            switch(internalAttatchmentFormat)
            {
                case FrameBufferAttachmentFormat::Color_Attachment:
                    attach.AttachmentFormat= GL_COLOR_ATTACHMENT0 + attachments.size();
                    attach.texture.CreateTexture(TextureType::Texture,TextureSubType::NormalTexture, Width, Height, mipmapping);
                    break;
                case FrameBufferAttachmentFormat::Color_Attachment_Floating_Point_16F:
                    attach.AttachmentFormat= GL_COLOR_ATTACHMENT0 + attachments.size();
                    attach.texture.CreateTexture(TextureType::Texture,TextureSubType::FloatingPointTexture16F, Width, Height, mipmapping);
                    break;
                case FrameBufferAttachmentFormat::Color_Attachment_Floating_Point_32F:
                    attach.AttachmentFormat= GL_COLOR_ATTACHMENT0 + attachments.size();
                    attach.texture.CreateTexture(TextureType::Texture,TextureSubType::FloatingPointTexture32F, Width, Height, mipmapping);
                    break;
                case FrameBufferAttachmentFormat::Depth_Attachment:
                    attach.AttachmentFormat= GL_DEPTH_ATTACHMENT;
                    attach.AttachmentFormat= GL_COLOR_ATTACHMENT0 + attachments.size();
                    attach.texture.CreateTexture(TextureType::Texture,TextureSubType::DepthComponent, Width, Height, mipmapping);
                    break;
                case FrameBufferAttachmentFormat::Stencil_Attachment:
                    attach.AttachmentFormat= GL_STENCIL_ATTACHMENT;
                    attach.AttachmentFormat= GL_COLOR_ATTACHMENT0 + attachments.size();
                    attach.texture.CreateTexture(TextureType::Texture,TextureSubType::NormalTexture, Width, Height, mipmapping);
                    break;
            };

            // bind FBO
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, attach.AttachmentFormat, GL_TEXTURE_2D, attach.texture.GetID() , 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Add Texture
            attachments.push_back(attach);
        }
        
    }
    
    void FrameBuffer::Resize(const unsigned& width, const unsigned& height)
    {        
        
        if (FBOInitialized==true)
        {
            // Save Dimensions
            Width = width;
            Height = height;
                                    
            // bind FBO
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            
            // bind RBO
            if (useRenderBuffer==true)
            {
                glBindRenderbuffer(GL_RENDERBUFFER, rbo);
                glRenderbufferStorage(GL_RENDERBUFFER, framebufferFormat, Width, Height);                
            }
            
            for (unsigned int i=0;i<attachments.size();i++)
            {
                attachments[i].texture.Resize(Width,Height);
            }

            // unbind RBO
            if (useRenderBuffer==true)
            {
                glBindRenderbuffer(GL_RENDERBUFFER, 0);
            }            
            
            // unbind FBO        
            glBindFramebuffer(GL_FRAMEBUFFER, 0);            
            
        }
              
    }        
    void FrameBuffer::Bind()
    {
        // bind fbo
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        
        // Set ViewPort
        glViewport(0,0,Width,Height);
        
        if (attachments.size()>1)
        {
            std::vector<GLenum> BufferIDs;

            for(unsigned i = 0; i < attachments.size(); i++)
            {
                if (attachments[i].AttachmentFormat!=GL_DEPTH_ATTACHMENT && attachments[i].AttachmentFormat!=GL_STENCIL_ATTACHMENT)
                BufferIDs.push_back(GL_COLOR_ATTACHMENT0 + i);
            };

            glDrawBuffers(BufferIDs.size(), &BufferIDs[0]);
        };
    }
    void FrameBuffer::UnBind()
    {
        // unbind fbo
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        
        for (int i=0;i<attachments.size();i++) 
            attachments[i].texture.UpdateMipmap();
    }    
    Texture FrameBuffer::GetTexture(const unsigned &TextureNumber)
    {
        // get Texture
        return attachments[TextureNumber].texture;
    }
    
    const unsigned &FrameBuffer::GetWidth() const
    {
        return Width;
    }
    const unsigned &FrameBuffer::GetHeight() const
    {
        return Height;
    }
    const unsigned &FrameBuffer::GetFrameBufferFormat() const
    {
        return framebufferFormat;
    }
}
