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
			Stencil,
			RGBA_Multisample,
			Depth_Multisample,
			Stencil_Multisample
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

	namespace FBOBufferBit
	{
		enum {
			Color = 0,
			Depth,
			Stencil
		};
	}

	namespace FBOFilter
	{
		enum {
			Linear = 0,
			Nearest
		};
	}

	class PYROS3D_API FBOAttachment
	{
	public:
		uint32 AttachmentFormatInternal;
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
		void Init(const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa = 0); // RenderBuffer
		void AddAttach(const uint32 attachmentFormat, const uint32 TextureType, Texture* attachment);
		void AddAttach(const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa = 0);
		void Resize(const uint32 Width, const uint32 Height);
		void Bind(const uint32 access = FBOAccess::Read_Write);
		bool IsBinded();
		uint32 GetBindID();
		void UnBind();

		void CheckFBOStatus();

		std::vector<FBOAttachment*> GetAttachments() const { return attachments; }

		const uint32 &GetFrameBufferFormat() const;

		bool IsInitialized() { return FBOInitialized; }

		static void EnableMultisample();
		static void DisableMultisample();
		static void BlitFrameBuffer(const uint32 initSrcX, const uint32 initSrcY, const uint32 endSrcX, const uint32 endSrcY, const uint32 initDestX, const uint32 initDestY, const uint32 endDestX, const uint32 endDestY, const uint32 mask, const uint32 filter);

	private:

		// Bound FBOs
		static std::vector<std::vector<FrameBuffer*> > BoundFBOs;

		// Binded
		bool isBinded;
		uint32 glAccessBinded;
		uint32 accessBinded;

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
		void AddAttachToVector(FBOAttachment* attach);
		std::vector<FBOAttachment*> attachments;

	};

}

#endif  /* FRAMEBUFFER_H */
