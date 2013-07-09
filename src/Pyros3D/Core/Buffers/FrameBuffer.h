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

    namespace FrameBufferTypes {
        enum {            
            Color = 0,
            Depth,
            Stencil
        };
    };
    
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
    
    struct Attachment
    {
        Texture texture;
        uint32 AttachmentFormat;
    };
    
    class FrameBuffer {
        public:
            FrameBuffer();      
            virtual ~FrameBuffer();
            
            void Init(const uint32 &width, const uint32 &height, const uint32 &frameBufferType, const uint32 &internalAttatchmentFormat, bool mipmapping = false);
            void AddAttach(const uint32 &internalAttatchmentFormat, bool mipmapping = false);
            void Resize(const uint32 &width, const uint32 &height);
            void Bind();
            void UnBind();
            
            Texture GetTexture(const uint32 &TextureNumber);
            
            const uint32 &GetWidth() const;
            const uint32 &GetHeight() const;
            const uint32 &GetFrameBufferFormat() const;
            
            bool IsInitialized() { return FBOInitialized; }
            
        private:
            
            // dimensions
            uint32 Width, Height;
            // FBO Type
            uint32 type;
            // Internal Format
            uint32 framebufferFormat;
            // Frame Buffer Object
            uint32 fbo;
            // render buffer object
            uint32 rbo;
            bool useRenderBuffer;
            // Flag
            bool FBOInitialized;

            // FBO "texture"
            std::vector<Attachment> attachments;
            
    };

}

#endif  /* FRAMEBUFFER_H */