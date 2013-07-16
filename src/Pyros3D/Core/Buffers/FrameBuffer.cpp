//============================================================================
// Name        : FrameBuffer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : FrameBuffer
//============================================================================

#include "FrameBuffer.h"
#include "GL/glew.h"

namespace p3d {

    FrameBuffer::FrameBuffer() 
    {
        FBOInitialized = false;
        useRenderBuffer = false;
    }

    FrameBuffer::~FrameBuffer() 
    {
        // destroy fbo and rbo
        glDeleteFramebuffers(1, (GLuint*)&fbo);
        
        if (useRenderBuffer==true)
            glDeleteRenderbuffers(1, (GLuint*)&rbo);
        
        for (int32 i=0;i<attachments.size();i++) 
            attachments[i].texture.DeleteTexture();
        
        // flag FBO Stoped
        FBOInitialized = false;
    }
    
    void FrameBuffer::Init(const uint32& width, const uint32& height, const uint32& frameBufferType, const uint32& internalAttatchmentFormat, bool mipmapping, bool RenderBuffer, bool DrawBuffers)
    {                
        
        // Save Dimensions
        Width = width;
        Height = height;
        
        // Use Render Buffer
        useRenderBuffer = RenderBuffer;
        
        if (FBOInitialized==true)
        {
            // destroy fbo and rbo
            glDeleteFramebuffers(1, (GLuint*)&fbo);

            if (useRenderBuffer==true)
                glDeleteRenderbuffers(1, (GLuint*)&rbo);

            for (int32 i=0;i<attachments.size();i++) 
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
                framebufferFormat = GL_DEPTH_COMPONENT;
                break;               
            case FrameBufferTypes::Stencil:
                framebufferFormat = GL_STENCIL_INDEX;
                break;
        };        
        
        glGenFramebuffers(1, (GLuint*)&fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        
        // create RBO
        if (useRenderBuffer==true)
        {
            glGenRenderbuffers(1, (GLuint*)&rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);                        
            glRenderbufferStorage(GL_RENDERBUFFER, framebufferFormat, Width, Height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        
        // Add Attachment
        AddAttach(internalAttatchmentFormat, mipmapping);

        glBindFramebuffer(GL_FRAMEBUFFER,fbo);

        if (!DrawBuffers)
        {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }
        
        switch ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) )
        {
            case GL_FRAMEBUFFER_COMPLETE:
            {
                echo("FBO: The framebuffer is complete and valid for rendering.");
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            {
                echo("FBO: One or more attachment points are not framebuffer attachment complete. This could mean theres no texture attached or the format isnt renderable. For color textures this means the base format must be RGB or RGBA and for depth textures it must be a DEPTH_COMPONENT format. Other causes of this error are that the width or height is zero or the z-offset is out of range in case of render to volume.");
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            {
                echo("FBO: There are no attachments.");
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            {
                echo("FBO: Attachments are of different size. All attachments must have the same width and height.");
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            {
                echo("FBO: The color attachments have different format. All color attachments must have the same format.");
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            {
                echo("FBO: An attachment point referenced by GL.DrawBuffers() doesn't have an attachment.");
                break;
            }
            case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            {
                echo("FBO: The attachment point referenced by GL.ReadBuffers() doesn't have an attachment.");
                break;
            }
            case GL_FRAMEBUFFER_UNSUPPORTED:
            {
                echo("FBO: This particular FBO configuration is not supported by the implementation.");
                break;
            }
            default:
            {
                echo("FBO: Status unknown. (yes, this is really bad.)");
                break;
            }
        }

        // unbind FBO and RBO  
        if (useRenderBuffer==true)
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
 
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
    }
    
    void FrameBuffer::AddAttach(const uint32& internalAttatchmentFormat, bool mipmapping)
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
                    attach.texture.CreateTexture(TextureType::Texture,TextureSubType::DepthComponent, Width, Height, mipmapping);
                    break;
                case FrameBufferAttachmentFormat::Stencil_Attachment:
                    attach.AttachmentFormat= GL_STENCIL_ATTACHMENT;
                    attach.texture.CreateTexture(TextureType::Texture,TextureSubType::NormalTexture, Width, Height, mipmapping);
                    break;
            };
            // bind FBO
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, attach.AttachmentFormat, GL_TEXTURE_2D, attach.texture.GetBindID() , 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Add Texture
            attachments.push_back(attach);
        }
        
    }
    
    void FrameBuffer::Resize(const uint32& width, const uint32& height)
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
            
            for (uint32 i=0;i<attachments.size();i++)
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

            for(uint32 i = 0; i < attachments.size(); i++)
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
        
        for (int32 i=0;i<attachments.size();i++) 
            attachments[i].texture.UpdateMipmap();
    }    
    Texture FrameBuffer::GetTexture(const uint32 &TextureNumber)
    {
        // get Texture
        return attachments[TextureNumber].texture;
    }
    
    const uint32 &FrameBuffer::GetWidth() const
    {
        return Width;
    }
    const uint32 &FrameBuffer::GetHeight() const
    {
        return Height;
    }
    const uint32 &FrameBuffer::GetFrameBufferFormat() const
    {
        return framebufferFormat;
    }
}
