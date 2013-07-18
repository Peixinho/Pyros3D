//============================================================================
// Name        : FrameBuffer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : FrameBuffer
//============================================================================

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "../../Materials/Shaders/Shaders.h"
#include "../../AssetManager/AssetManager.h"
#include "../../AssetManager/Assets/Texture/Texture.h"
#include "../Logs/Log.h"

#define GLCHECK() { int32 error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }

namespace p3d {
    
    namespace FrameBufferAttachmentFormat
    {
        enum {
            Color_Attachment = 0,
            Color_Attachment_Floating_Point_16F,
            Color_Attachment_Floating_Point_32F,
            Depth_Attachment,
            Stencil_Attachment
        };
    }
    
    namespace RenderBufferType
    {
        enum {
            Color = 0,
            Depth,
            Stencil
        };
    }
    
    struct Attachment
    {
        Texture *TexturePTR;
        uint32 AttachmentFormat;
        uint32 TextureType;
    };
    
    class FrameBuffer {
        public:
            FrameBuffer();      
            virtual ~FrameBuffer();
            
            void Init(const uint32 &attachmentFormat, const uint32 &TextureType, Texture* attachment, bool DrawBuffers = true);
            void Init(const uint32 &attachmentFormat, const uint32 &TextureType, Texture* attachment, const uint32& BufferFormat, const uint32 &width, const uint32 &height, bool DrawBuffers);
            void AddAttach(const uint32& attachmentFormat, const uint32 &TextureType, Texture* attachment);
            void ResizeRenderBuffer(const uint32 &width, const uint32 &height);
            void Bind();
            uint32 GetBindID();
            void UnBind();
            
            const uint32 &GetFrameBufferFormat() const;
            
            bool IsInitialized() { return FBOInitialized; }
            
            
        private:

            // FBO Type
            uint32 type;
            // Internal Format
            uint32 framebufferFormat;
            // Frame Buffer Object
            uint32 fbo;
            // DrawBuffers
            bool drawBuffers;
            // RenderBuffer
            uint32 rbo;
            uint32 rboType;
            uint32 rboAttachment;
            uint32 rboWidth,rboHeight;
            bool isUsingRenderBuffer;
            
            // Flags
            bool FBOInitialized;

            // FBO "texture"
            std::vector<Attachment> attachments;
            
    };

}

#endif  /* FRAMEBUFFER_H */