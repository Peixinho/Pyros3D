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

	// Engine-neutral form of the glCheckFramebufferStatus() result, so
	// CheckFBOStatus() can switch without depending on raw GL_FRAMEBUFFER_*
	// tokens - see IRenderDevice::TranslateFramebufferStatus().
	namespace FBOStatus
	{
		enum {
			Complete = 0,
			IncompleteAttachment,
			IncompleteMissingAttachment,
			IncompleteDrawBuffer,
			IncompleteReadBuffer,
			Unsupported,
			Unknown
		};
	}

	class PYROS3D_API FBOAttachment
	{
	public:
		// Two of these used to be named as if they held engine enums while
		// actually holding backend-translated values, which cost two separate
		// debugging sessions on the render-target viewer: a GL depth
		// attachment reports GL_DEPTH_ATTACHMENT (36096) where
		// FrameBufferAttachmentFormat::Depth_Attachment is 16, and a 2D
		// colour target reports GL_TEXTURE_2D (3553) where
		// TextureType::Texture is 7. Vulkan does not translate the
		// attachment format at all, so the same comparison "worked" there
		// and failed on GL. The Native prefix is the whole point: compare
		// these against nothing, pass them to the device.
		//
		// EngineAttachmentFormat is the one that can be compared - it is the
		// FrameBufferAttachmentFormat::* the caller actually passed. It was
		// called AttachmentFormatInternal, which reads like GL's
		// `internalFormat` (a pixel format) and means the opposite of what it
		// says.
		uint32 EngineAttachmentFormat;
		uint32 NativeAttachmentFormat;
		uint32 AttachmentType;

		// Texture Specific - TexturePTR is borrowed (owned by whoever called
		// AddAttach), never freed here.
		Texture *TexturePTR;
		// Native target (GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP_POSITIVE_X, ...),
		// not TextureType::*. For the engine value ask the texture:
		// TexturePTR->GetTextureType().
		uint32 NativeTextureTarget;

		// RenderBuffer Specific - rboID is owned; released in the destructor.
		uint32 Width;
		uint32 Height;
		uint32 rboID;
		uint32 DataType;

		~FBOAttachment();
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

		// Every live FrameBuffer, in creation order. Registered by the
		// constructor and dropped by the destructor, so anything that
		// creates a render target appears here without having to be told
		// about it - which is the point: a debug view built on this stays
		// correct as renderers and effects come and go.
		static const std::vector<FrameBuffer*> &GetLiveFrameBuffers() { return LiveFBOs; }

		static void EnableMultisample();
		static void DisableMultisample();
		static void BlitFrameBuffer(const uint32 initSrcX, const uint32 initSrcY, const uint32 endSrcX, const uint32 endSrcY, const uint32 initDestX, const uint32 initDestY, const uint32 endDestX, const uint32 endDestY, const uint32 mask, const uint32 filter);

	private:

		// Bound FBOs
		static std::vector<std::vector<FrameBuffer*> > BoundFBOs;
		// See GetLiveFrameBuffers().
		static std::vector<FrameBuffer*> LiveFBOs;

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
