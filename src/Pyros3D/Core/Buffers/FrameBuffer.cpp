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
        isBinded = false;
    }

    FrameBuffer::~FrameBuffer() 
    {
        
        // flag FBO Stoped
        FBOInitialized = false;
        
        if (isUsingRenderBuffer)
        {
            glDeleteRenderbuffers(1, (GLuint*)&rbo);
        }
        
        // destroy fbo and rbo
        glDeleteFramebuffers(1, (GLuint*)&fbo);
        
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
        
        if (isUsingRenderBuffer)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glGenRenderbuffers(1, (GLuint*)&rbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, rboType, rboWidth, rboHeight);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, rboAttachment, GL_RENDERBUFFER, rbo);
        }
        
        // Add Attach
        AddAttach(attachmentFormat, TextureType, attachment);
        
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
        
        if (isUsingRenderBuffer)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
    }
    
    void FrameBuffer::Init(const uint32 &attachmentFormat, const uint32 &TextureType, p3d::Texture *attachment, const uint32 &BufferFormat, const uint32 &width, const uint32 &height, bool DrawBuffers)
    {
        switch(BufferFormat)
        {
            case RenderBufferType::Depth:
                rboType = GL_DEPTH_COMPONENT;
                rboAttachment = GL_DEPTH_ATTACHMENT;
                break;
            case RenderBufferType::Stencil:
                rboType = GL_STENCIL_INDEX;
                rboAttachment = GL_STENCIL_ATTACHMENT;
                break;
            case RenderBufferType::Color:
            default:
                rboType = GL_RGBA8;
                rboAttachment = GL_RGBA;
                break;
        };
        
        isUsingRenderBuffer = true;
        rboWidth = width;
        rboHeight = height;
    
        Init(attachmentFormat, TextureType, attachment,DrawBuffers);

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
            case FrameBufferAttachmentFormat::Color_Attachment0:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT0;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment1:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT1;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment2:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT2;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment3:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT3;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment4:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT4;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment5:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT5;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment6:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT6;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment7:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT7;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment8:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT8;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment9:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT9;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment10:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT10;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment11:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT11;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment12:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT12;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment13:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT13;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment14:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT14;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment15:
                attach.AttachmentFormat= GL_COLOR_ATTACHMENT15;
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
        
        attachments[attachmentFormat] = attach;
        
        if (!isBinded)
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        
        // Add Attach
        glFramebufferTexture2D(GL_FRAMEBUFFER, attach.AttachmentFormat, attach.TextureType, attach.TexturePTR->GetBindID() , 0);
        
        if (!isBinded)
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
        
        if (attachments.size()>1 && drawBuffers)
        {
            std::vector<GLenum> BufferIDs;

            for(uint32 i = FrameBufferAttachmentFormat::Color_Attachment0; i < FrameBufferAttachmentFormat::Color_Attachment15; i++)
            {
                BufferIDs.push_back(GL_COLOR_ATTACHMENT0 + i);
            };

            glDrawBuffers(BufferIDs.size(), &BufferIDs[0]);
        };
        
        isBinded = true;
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
        glReadBuffer(GL_BACK);
        for (std::map<uint32, Attachment>::iterator i = attachments.begin();i!=attachments.end();i++) 
            (*i).second.TexturePTR->UpdateMipmap();
        
        isBinded = false;
    }
    bool FrameBuffer::IsBinded()
    {
        return isBinded;
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
