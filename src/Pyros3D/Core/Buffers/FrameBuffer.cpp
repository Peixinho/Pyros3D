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
    }

    FrameBuffer::~FrameBuffer() 
    {
        // destroy fbo and rbo
        glDeleteFramebuffers(1, (GLuint*)&fbo);
        
        // flag FBO Stoped
        FBOInitialized = false;
        
        if (isUsingRenderBuffer)
        {
            glDeleteRenderbuffers(1, (GLuint*)&rbo);
        }
        
    }
    
    void FrameBuffer::Init(const uint32& attachmentFormat, const uint32 &TextureType, Texture* attachment, bool DrawBuffers)
    {                

        if (FBOInitialized==true)
        {
            // destroy fbo and rbo
            glDeleteFramebuffers(1, (GLuint*)&fbo);
        
            // flag FBO Stoped
            FBOInitialized = false;
        }
        
        // flag FBO Initialized
        FBOInitialized = true;

        glGenFramebuffers(1, (GLuint*)&fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        // Add Attach
        AddAttach(attachmentFormat, TextureType, attachment);
        
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        
        // Save Flag
        drawBuffers = DrawBuffers;
        
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
 
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
    }
    
    void FrameBuffer::AddAttach(const uint32& attachmentFormat, const uint32 &TextureType, Texture* attachment)
    {
        // Add Attachment
        Attachment attach;
        attach.AttachmentFormat = attachmentFormat;
        attach.TexturePTR = attachment;
        attach.TextureType = TextureType;

        // Get Attatchment Format
        switch(attach.AttachmentFormat)
        {
            case FrameBufferAttachmentFormat::Color_Attachment:
            case FrameBufferAttachmentFormat::Color_Attachment_Floating_Point_16F:
            case FrameBufferAttachmentFormat::Color_Attachment_Floating_Point_32F:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT0 + attachments.size();
                break;
            case FrameBufferAttachmentFormat::Depth_Attachment:
                attach.AttachmentFormat= GL_DEPTH_ATTACHMENT;
                break;
            case FrameBufferAttachmentFormat::Stencil_Attachment:
                attach.AttachmentFormat= GL_STENCIL_ATTACHMENT;
                break;
        };

        switch(TextureType)
        {
            case TextureType::CubemapNegative_X:
                attach.TextureType=GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
                break;
            case TextureType::CubemapNegative_Y:
                attach.TextureType=GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
                break;
            case TextureType::CubemapNegative_Z:
                attach.TextureType=GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
                break;
            case TextureType::CubemapPositive_X:
                attach.TextureType=GL_TEXTURE_CUBE_MAP_POSITIVE_X;
                break;
            case TextureType::CubemapPositive_Y:
                attach.TextureType=GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
                break;
            case TextureType::CubemapPositive_Z:
                attach.TextureType=GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
                break;
            case TextureType::Texture:
            default:
                attach.TextureType=GL_TEXTURE_2D;
                break;
        }
        
        attachments.push_back(attach);
        
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        
        // Add Attach
        glFramebufferTexture2D(GL_FRAMEBUFFER, attach.AttachmentFormat, attach.TextureType, attach.TexturePTR->GetBindID() , 0);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    void FrameBuffer::Bind()
    {
        // bind fbo
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        
        if (!drawBuffers)
        {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }
        
        if (attachments.size()>1)
        {
            std::vector<GLenum> BufferIDs;

            uint32 k = 0;
            for(uint32 i = 0; i < attachments.size(); i++)
            {
                if (attachments[i].AttachmentFormat!=GL_DEPTH_ATTACHMENT && attachments[i].AttachmentFormat!=GL_STENCIL_ATTACHMENT)
                {
                    BufferIDs.push_back(GL_COLOR_ATTACHMENT0 + k);
                    k++;
                }
            };

            glDrawBuffers(BufferIDs.size(), &BufferIDs[0]);
        };
    }
    uint32 FrameBuffer::GetBindID()
    {
        return fbo;
    }
    void FrameBuffer::UnBind()
    {
        // unbind fbo
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDrawBuffer(GL_BACK);
        
        for (int32 i=0;i<attachments.size();i++) 
            attachments[i].TexturePTR->UpdateMipmap();
    }
    
    void FrameBuffer::AddRenderBuffer(const uint32& BufferFormat, const uint32 &width, const uint32 &height)
    {
        if(!isUsingRenderBuffer)
        {
            switch(BufferFormat)
            {
                case RenderBufferType::Color:
                    rboType = GL_RGBA8;
                    break; 
                case RenderBufferType::Depth:
                    rboType = GL_DEPTH_COMPONENT;
                    break;
                case RenderBufferType::Stencil:
                    rboType = GL_STENCIL_ATTACHMENT;
                    break;                
            };
            
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glGenRenderbuffers(1, (GLuint*)&rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);                        
            glRenderbufferStorage(GL_RENDERBUFFER, rboType, width,height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            
            isUsingRenderBuffer = true;
        }
    }
    
    void FrameBuffer::ResizeRenderBuffer(const uint32& width, const uint32& height)
    {
        if (isUsingRenderBuffer)
        {            
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, rboType, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0); 
        }
    }
    
    const uint32 &FrameBuffer::GetFrameBufferFormat() const
    {
        return framebufferFormat;
    }
}
