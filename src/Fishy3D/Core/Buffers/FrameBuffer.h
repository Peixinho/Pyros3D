//============================================================================
// Name        : FrameBuffer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : FrameBuffer
//============================================================================

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "../../Utils/Pointers/SuperSmartPointer.h"
#include "../../Materials/Shaders/Shaders.h"
#include "../../Textures/Texture.h"

#define GLCHECK() { int error = glGetError(); if(error != GL_NO_ERROR) { std::cout <<  "GL Error: " << std::hex << error << std::endl; } }

namespace Fishy3D {

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
        unsigned AttachmentFormat;
    };
    
    class FrameBuffer {
        public:
            FrameBuffer();      
            virtual ~FrameBuffer();
            
            void Init(const unsigned int &width, const unsigned int &height, const unsigned int &frameBufferType, const unsigned int &internalAttatchmentFormat, bool mipmapping = false);
            void AddAttach(const unsigned int &internalAttatchmentFormat, bool mipmapping = false);
            void Resize(const unsigned &width, const unsigned &height);
            void Bind();
            void UnBind();
            
            Texture GetTexture(const unsigned &TextureNumber);
            
            const unsigned &GetWidth() const;
            const unsigned &GetHeight() const;
            const unsigned &GetFrameBufferFormat() const;
            
        private:
            
            // dimensions
            unsigned Width, Height;
            // FBO Type
            unsigned int type;
            // Internal Format
            unsigned int framebufferFormat;
            // Frame Buffer Object
            unsigned fbo;
            // render buffer object
            unsigned rbo;
            bool useRenderBuffer;
            // Flag
            bool FBOInitialized;

            // FBO "texture"
            std::vector<Attachment> attachments;
            
    };

}

#endif  /* FRAMEBUFFER_H */