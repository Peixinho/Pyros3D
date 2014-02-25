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
            Color_Attachment0 = 0,
            Color_Attachment1,
            Color_Attachment2,
            Color_Attachment3,
            Color_Attachment4,
            Color_Attachment5,
            Color_Attachment6,
            Color_Attachment7,
            Color_Attachment8,
            Color_Attachment9,
            Color_Attachment10,
            Color_Attachment11,
            Color_Attachment12,
            Color_Attachment13,
            Color_Attachment14,
            Color_Attachment15,
            Depth_Attachment,
            Depth_Attachment16,
            Depth_Attachment24,
            Depth_Attachment32,
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
            bool IsBinded();
            uint32 GetBindID();
            void UnBind();
            
            const uint32 &GetFrameBufferFormat() const;
            
            bool IsInitialized() { return FBOInitialized; }
            
            
        private:

            // Binded
            bool isBinded;
            
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
            std::map<uint32, Attachment*> attachments;
            
    };

}

#endif  /* FRAMEBUFFER_H */