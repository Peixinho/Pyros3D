//============================================================================
// Name        : FrameBuffer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : FrameBuffer
//============================================================================

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H
#include <Pyros3D/Materials/Shaders/Shaders.h>
#include <Pyros3D/Assets/Texture/Texture.h>
#include <Pyros3D/Core/Logs/Log.h>
#include <Pyros3D/Other/Export.h>
#include <vector>

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
            Stencil_Attachment
        };
    }

    namespace RenderBufferDataType
    {
        enum {
            RGBA = 0,
            Depth,
            Stencil
        };
    }
    
    namespace FBOAttachmentType
    {
        enum {
            Texture = 0,
            RenderBuffer
        };
    }

	namespace FBOAccess {
		enum {
			Read_Write = 0,
			Read,
			Write
		};
	}

    class PYROS3D_API FBOAttachment
    {
        public:
            uint32 AttachmentFormat;
            uint32 AttachmentType;

            // Texture Specific
            Texture *TexturePTR;
            uint32 TextureType;

            // RenderBuffer Specific
            uint32 Width;
            uint32 Height;
            uint32 rboID;
            uint32 DataType;
    };
    
    class PYROS3D_API FrameBuffer {
        public:			

            FrameBuffer();
            virtual ~FrameBuffer();
            
            void Init(const uint32 attachmentFormat, const uint32 TextureType, Texture* attachment); // Using Textures
            void Init(const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height); // RenderBuffer
            void AddAttach(const uint32 attachmentFormat, const uint32 TextureType, Texture* attachment);
            void AddAttach(const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height);
            void Resize(const uint32 Width, const uint32 Height);
            void Bind(const uint32 access = FBOAccess::Read_Write);
            bool IsBinded();
            uint32 GetBindID();
            void UnBind();

            void CheckFBOStatus();

            std::map<uint32, FBOAttachment*> GetAttachments() const { return attachments; }
            
            const uint32 &GetFrameBufferFormat() const;
            
            bool IsInitialized() { return FBOInitialized; }
            
        private:

			// Bound FBOs
			static std::map<uint32, std::vector<FrameBuffer*>> BoundFBOs;

            // Binded
            bool isBinded;
			uint32 glAccessBinded;
            
            // FBO Type
            uint32 type;
            // Internal Format
            uint32 framebufferFormat;
            // Frame Buffer Object
            uint32 fbo;
            // DrawBuffers
            bool drawBuffers;
            
            // Flags
            bool FBOInitialized;

            // FBO "texture"
            std::map<uint32, FBOAttachment*> attachments;
            
    };

}

#endif  /* FRAMEBUFFER_H */