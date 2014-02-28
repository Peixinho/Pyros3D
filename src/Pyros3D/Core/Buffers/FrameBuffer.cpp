//============================================================================
// Name        : FrameBuffer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : FrameBuffer
//============================================================================

#include "FrameBuffer.h"
#ifdef ANDROID
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
#else
    #include "GL/glew.h"
#endif

namespace p3d {

    FrameBuffer::FrameBuffer() 
    {
        FBOInitialized = false;
        isBinded = false;
        // FBO Type
        type = 0;
        // Internal Format
        framebufferFormat = 0;
        // Frame Buffer Object
        fbo = 0;
        // DrawBuffers
        drawBuffers = false;
    }

    FrameBuffer::~FrameBuffer()
    {
        
        // flag FBO Stoped
        FBOInitialized = false;
        
        // destroy fbo and rbo
        glDeleteFramebuffers(1, (GLuint*)&fbo);
        
        for (std::map<uint32, FBOAttachment*>::iterator i=attachments.begin();i!=attachments.end();i++)
            delete (*i).second;
        
        attachments.clear();
        
    }
    void FrameBuffer::Init(const uint32 &attachmentFormat, const uint32 &TextureType, p3d::Texture *attachment)
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

        isBinded = false;

        // Add Attach
        AddAttach(attachmentFormat, TextureType, attachment);

    }

    void FrameBuffer::Init(const uint32& attachmentFormat, const uint32 &attachmentDataType, const uint32 Width, const uint32 &Height)
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

        isBinded = false;

        // Add Attach
        AddAttach(attachmentFormat, attachmentDataType, Width, Height);

    }
    
    void FrameBuffer::CheckFBOStatus()
    {
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
#ifndef ANDROID
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
#endif
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
    }
    
    void FrameBuffer::AddAttach(const uint32& attachmentFormat, const uint32 &TextureType, Texture* attachment)
    {
        // Add Attachment
        FBOAttachment* attach = new FBOAttachment();
        attach->AttachmentFormat = attachmentFormat;
        attach->AttachmentType = FBOAttachmentType::Texture;
        attach->TexturePTR = attachment;
        attach->TextureType = TextureType;
        
        // Get Attatchment Format
        switch(attach->AttachmentFormat)
        {
            case FrameBufferAttachmentFormat::Color_Attachment0:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT0;
                break;
#ifndef ANDROID
            case FrameBufferAttachmentFormat::Color_Attachment1:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT1;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment2:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT2;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment3:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT3;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment4:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT4;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment5:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT5;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment6:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT6;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment7:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT7;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment8:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT8;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment9:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT9;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment10:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT10;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment11:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT11;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment12:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT12;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment13:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT13;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment14:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT14;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment15:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT15;
                break;
#endif
            case FrameBufferAttachmentFormat::Depth_Attachment:
                attach->AttachmentFormat= GL_DEPTH_ATTACHMENT;
                break;
            case FrameBufferAttachmentFormat::Stencil_Attachment:
                attach->AttachmentFormat= GL_STENCIL_ATTACHMENT;
                break;
        };

        if (attachmentFormat >= FrameBufferAttachmentFormat::Color_Attachment0 && attachmentFormat <= FrameBufferAttachmentFormat::Color_Attachment15 && drawBuffers == false)
            drawBuffers = true;

        switch(TextureType)
        {
            case TextureType::CubemapNegative_X:
                attach->TextureType=GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
                break;
            case TextureType::CubemapNegative_Y:
                attach->TextureType=GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
                break;
            case TextureType::CubemapNegative_Z:
                attach->TextureType=GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
                break;
            case TextureType::CubemapPositive_X:
                attach->TextureType=GL_TEXTURE_CUBE_MAP_POSITIVE_X;
                break;
            case TextureType::CubemapPositive_Y:
                attach->TextureType=GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
                break;
            case TextureType::CubemapPositive_Z:
                attach->TextureType=GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
                break;
            case TextureType::Texture:
            default:
                attach->TextureType=GL_TEXTURE_2D;
                break;
        }
        
        attachments[attachmentFormat] = attach;
        
        if (!isBinded)
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        
        // Add Attach
        glFramebufferTexture2D(GL_FRAMEBUFFER, attach->AttachmentFormat, attach->TextureType, attach->TexturePTR->GetBindID() , 0);

#ifndef ANDROID
        
        if (!drawBuffers)
        {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }

        if (attachments.size()>0 && drawBuffers)
        {
            std::vector<GLenum> BufferIDs;
            uint32 counter = 0;
            for(std::map<uint32,FBOAttachment*>::iterator i = attachments.begin(); i != attachments.end(); i++)
            {
                if ((*i).first >= FrameBufferAttachmentFormat::Color_Attachment0 && (*i).first <= FrameBufferAttachmentFormat::Color_Attachment15) {
                    BufferIDs.push_back(GL_COLOR_ATTACHMENT0 + counter++);
                }
            }

            glDrawBuffers(BufferIDs.size(), &BufferIDs[0]);
        }

#endif

        CheckFBOStatus();

        if (!isBinded)
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void FrameBuffer::AddAttach(const uint32& attachmentFormat, const uint32 &attachmentDataType, const uint32 &Width, const uint32 &Height)
    {
        // Add Attachment
        FBOAttachment* attach = new FBOAttachment();
        attach->AttachmentFormat = attachmentFormat;
        attach->AttachmentType = FBOAttachmentType::RenderBuffer;
        attach->Width = Width;
        attach->Height = Height;
        attach->DataType = attachmentDataType;

        glGenRenderbuffers (1, (GLuint*)&attach->rboID);
        glBindRenderbuffer (GL_RENDERBUFFER, attach->rboID);

        // Get Attatchment Format
        switch(attach->AttachmentFormat)
        {
            case FrameBufferAttachmentFormat::Color_Attachment0:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT0;
                break;
#ifndef ANDROID
            case FrameBufferAttachmentFormat::Color_Attachment1:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT1;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment2:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT2;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment3:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT3;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment4:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT4;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment5:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT5;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment6:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT6;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment7:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT7;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment8:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT8;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment9:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT9;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment10:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT10;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment11:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT11;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment12:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT12;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment13:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT13;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment14:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT14;
                break;
            case FrameBufferAttachmentFormat::Color_Attachment15:
                attach->AttachmentFormat= GL_COLOR_ATTACHMENT15;
                break;
#endif
            case FrameBufferAttachmentFormat::Depth_Attachment:
                attach->AttachmentFormat= GL_DEPTH_ATTACHMENT;
                break;
            case FrameBufferAttachmentFormat::Stencil_Attachment:
                attach->AttachmentFormat= GL_STENCIL_ATTACHMENT;
                break;
        };

        switch(attach->DataType)
        {
            case RenderBufferDataType::Depth:
                attach->DataType = GL_DEPTH_COMPONENT;
            break;
            // case RenderBufferDataType::Stencil:
            //     attach->DataType = GL_STENCIL_COMPONENT;
            // break;
            case RenderBufferDataType::RGBA:
            default:
                attach->DataType = GL_RGBA;
            break;
        }

        attachments[attachmentFormat] = attach;
        
        if (!isBinded)
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Add RenderBuffer
        glRenderbufferStorage (GL_RENDERBUFFER, attach->DataType, attach->Width, attach->Height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attach->AttachmentFormat, GL_RENDERBUFFER, attach->rboID);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        CheckFBOStatus();

#ifndef ANDROID
        
        if (!drawBuffers)
        {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }

        if (attachments.size()>0 && drawBuffers)
        {
            std::vector<GLenum> BufferIDs;
            uint32 counter = 0;
            for(std::map<uint32,FBOAttachment*>::iterator i = attachments.begin(); i != attachments.end(); i++)
            {
                if ((*i).first >= FrameBufferAttachmentFormat::Color_Attachment0 && (*i).first <= FrameBufferAttachmentFormat::Color_Attachment15) {
                    BufferIDs.push_back(GL_COLOR_ATTACHMENT0 + counter++);
                }
            }

            glDrawBuffers(BufferIDs.size(), &BufferIDs[0]);
        }

#endif

        if (!isBinded)
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }    
    
    void FrameBuffer::Bind()
    {
        // bind fbo
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
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

#ifndef ANDROID
        if (drawBuffers)
        {
            glDrawBuffer(GL_BACK);
            glReadBuffer(GL_BACK);
        }
#endif

        for (std::map<uint32, FBOAttachment*>::iterator i=attachments.begin();i!=attachments.end();i++)
        {
            if ((*i).second->AttachmentType==FBOAttachmentType::Texture)
            {
                (*i).second->TexturePTR->UpdateMipmap();
            }
        }
        
        isBinded = false;
    }
    bool FrameBuffer::IsBinded()
    {
        return isBinded;
    }
    
    void FrameBuffer::Resize(const uint32 &Width, const uint32 &Height)
    {
        for (std::map<uint32, FBOAttachment*>::iterator i=attachments.begin();i!=attachments.end();i++)
        {
            if ((*i).second->AttachmentType==FBOAttachmentType::RenderBuffer)
            {
                glBindRenderbuffer(GL_RENDERBUFFER, (*i).second->rboID);
                glRenderbufferStorage(GL_RENDERBUFFER, (*i).second->DataType, Width, Height);
                glBindRenderbuffer(GL_RENDERBUFFER, 0);
            }
        }
    }

    const uint32 &FrameBuffer::GetFrameBufferFormat() const
    {
        return framebufferFormat;
    }
}
