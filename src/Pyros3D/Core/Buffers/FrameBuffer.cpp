//============================================================================
// Name        : FrameBuffer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : FrameBuffer
//============================================================================

#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

	// 3 types of bound framebuffers (Read, Write and Read_Write)
	std::vector< std::vector<FrameBuffer*> > FrameBuffer::BoundFBOs(3);

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
		GLCHECKER(glDeleteFramebuffers(1, (GLuint*)&fbo));

		for (std::vector<FBOAttachment*>::iterator i = attachments.begin(); i != attachments.end(); i++)
			delete (*i);

		attachments.clear();

	}
	void FrameBuffer::BlitFrameBuffer(const uint32 initSrcX, const uint32 initSrcY, const uint32 endSrcX, const uint32 endSrcY, const uint32 initDestX, const uint32 initDestY, const uint32 endDestX, const uint32 endDestY, const uint32 mask, const uint32 filter)
	{
#if !defined(GLES2)
		GLuint Mask;
		switch (mask)
		{
		case FBOBufferBit::Depth:
			Mask = GL_DEPTH_BUFFER_BIT;
			break;
		case FBOBufferBit::Stencil:
			Mask = GL_STENCIL_BUFFER_BIT;
			break;
		case FBOBufferBit::Color:
		default:
			Mask = GL_COLOR_BUFFER_BIT;
			break;
		};
		GLuint Filter;
		switch (filter)
		{
		case FBOFilter::Nearest:
			Filter = GL_NEAREST;
			break;
		case FBOFilter::Linear:
		default:
			Filter = GL_LINEAR;
			break;
		};
		GLCHECKER(glBlitFramebuffer(initSrcX, initSrcY, endSrcX, endSrcY, initDestX, initDestY, endDestX, endDestY, Mask, Filter));
#endif
	}
	void FrameBuffer::EnableMultisample()
	{
#if !defined(GLES2) && !defined(GLES3)
		GLCHECKER(glEnable(GL_MULTISAMPLE));
#endif
	}
	void FrameBuffer::DisableMultisample()
	{
#if !defined(GLES2) && !defined(GLES3)
		GLCHECKER(glDisable(GL_MULTISAMPLE));
#endif
	}
	void FrameBuffer::Init(const uint32 attachmentFormat, const uint32 TextureType, p3d::Texture *attachment)
	{

		if (FBOInitialized == true)
		{
			// destroy fbo and rbo
			GLCHECKER(glDeleteFramebuffers(1, (GLuint*)&fbo));

			// flag FBO Stoped
			FBOInitialized = false;
		}

		// flag FBO Initialized
		FBOInitialized = true;

		GLCHECKER(glGenFramebuffers(1, (GLuint*)&fbo));

		isBinded = false;

		// Add Attach
		AddAttach(attachmentFormat, TextureType, attachment);

	}
	void FrameBuffer::Init(const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa)
	{

		if (FBOInitialized == true)
		{
			// destroy fbo and rbo
			GLCHECKER(glDeleteFramebuffers(1, (GLuint*)&fbo));

			// flag FBO Stoped
			FBOInitialized = false;
		}

		// flag FBO Initialized
		FBOInitialized = true;

		GLCHECKER(glGenFramebuffers(1, (GLuint*)&fbo));

		isBinded = false;

		// Add Attach
		AddAttach(attachmentFormat, attachmentDataType, Width, Height, msaa);

	}
	void FrameBuffer::CheckFBOStatus()
	{
		switch (glCheckFramebufferStatus(GL_FRAMEBUFFER))
		{
		case GL_FRAMEBUFFER_COMPLETE:
		{

#ifdef _DEBUG
			echo("FBO: The framebuffer is complete and valid for rendering.");
#endif
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
#if !defined(GLES2) && !defined(GLES3)
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
	void FrameBuffer::AddAttach(const uint32 attachmentFormat, const uint32 TextureType, Texture* attachment)
	{
		// Add Attachment
		FBOAttachment* attach = new FBOAttachment();
		attach->AttachmentFormatInternal = attachmentFormat;
		attach->AttachmentType = FBOAttachmentType::Texture;
		attach->TexturePTR = attachment;
		attach->TextureType = TextureType;

		// Get Attatchment Format
		switch (attach->AttachmentFormatInternal)
		{
		case FrameBufferAttachmentFormat::Color_Attachment0:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT0;
			break;
#if !defined(GLES2)
		case FrameBufferAttachmentFormat::Color_Attachment1:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT1;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment2:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT2;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment3:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT3;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment4:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT4;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment5:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT5;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment6:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT6;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment7:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT7;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment8:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT8;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment9:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT9;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment10:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT10;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment11:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT11;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment12:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT12;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment13:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT13;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment14:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT14;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment15:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT15;
			break;
#endif
		case FrameBufferAttachmentFormat::Depth_Attachment:
			attach->AttachmentFormat = GL_DEPTH_ATTACHMENT;
			break;
		case FrameBufferAttachmentFormat::Stencil_Attachment:
			attach->AttachmentFormat = GL_STENCIL_ATTACHMENT;
			break;
		};

		if (attach->AttachmentFormatInternal >= FrameBufferAttachmentFormat::Color_Attachment0 && attachmentFormat <= FrameBufferAttachmentFormat::Color_Attachment15 && !drawBuffers)
			drawBuffers = true;

		switch (TextureType)
		{
		case TextureType::CubemapNegative_X:
			attach->TextureType = GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
			break;
		case TextureType::CubemapNegative_Y:
			attach->TextureType = GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
			break;
		case TextureType::CubemapNegative_Z:
			attach->TextureType = GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
			break;
		case TextureType::CubemapPositive_X:
			attach->TextureType = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
			break;
		case TextureType::CubemapPositive_Y:
			attach->TextureType = GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
			break;
		case TextureType::CubemapPositive_Z:
			attach->TextureType = GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
			break;
#if !defined(GLES2) && !defined(GLES3)
		case TextureType::Texture_Multisample:
			attach->TextureType = GL_TEXTURE_2D_MULTISAMPLE;
			break;
#endif
		case TextureType::Texture:
		default:
			attach->TextureType = GL_TEXTURE_2D;
			break;
		}

		AddAttachToVector(attach);

		if (!isBinded)
			GLCHECKER(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

		// Add Attach
		GLCHECKER(glFramebufferTexture2D(GL_FRAMEBUFFER, attach->AttachmentFormat, attach->TextureType, attach->TexturePTR->GetBindID(), 0));

#if !defined(GLES2) && !defined(GLES3)

		if (!drawBuffers)
		{
			GLCHECKER(glDrawBuffer(GL_NONE));
			GLCHECKER(glReadBuffer(GL_NONE));
		}

		if (attachments.size() > 0 && drawBuffers)
		{
			std::vector<GLenum> BufferIDs;
			uint32 counter = 0;
			for (std::vector<FBOAttachment*>::iterator i = attachments.begin(); i != attachments.end(); i++)
			{
				if ((*i)->AttachmentFormatInternal >= FrameBufferAttachmentFormat::Color_Attachment0 && (*i)->AttachmentFormatInternal <= FrameBufferAttachmentFormat::Color_Attachment15) {
					BufferIDs.push_back(GL_COLOR_ATTACHMENT0 + counter++);
				}
			}

			GLCHECKER(glDrawBuffers(BufferIDs.size(), &BufferIDs[0]));
		}
#endif

		CheckFBOStatus();

		if (!isBinded)
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	void FrameBuffer::AddAttach(const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa)
	{

		// Add Attachment
		FBOAttachment* attach = new FBOAttachment();
		attach->AttachmentFormatInternal = attachmentFormat;
		attach->AttachmentType = FBOAttachmentType::RenderBuffer;
		attach->Width = Width;
		attach->Height = Height;
		attach->DataType = attachmentDataType;

		if (!isBinded)
			GLCHECKER(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

		GLCHECKER(glGenRenderbuffers(1, (GLuint*)&attach->rboID));
		GLCHECKER(glBindRenderbuffer(GL_RENDERBUFFER, attach->rboID));

		// Get Attatchment Format
		switch (attach->AttachmentFormatInternal)
		{
		case FrameBufferAttachmentFormat::Color_Attachment0:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT0;
			break;
#if !defined(GLES2)
		case FrameBufferAttachmentFormat::Color_Attachment1:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT1;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment2:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT2;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment3:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT3;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment4:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT4;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment5:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT5;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment6:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT6;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment7:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT7;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment8:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT8;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment9:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT9;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment10:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT10;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment11:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT11;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment12:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT12;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment13:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT13;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment14:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT14;
			break;
		case FrameBufferAttachmentFormat::Color_Attachment15:
			attach->AttachmentFormat = GL_COLOR_ATTACHMENT15;
			break;
#endif
		case FrameBufferAttachmentFormat::Depth_Attachment:
			attach->AttachmentFormat = GL_DEPTH_ATTACHMENT;
			break;
		case FrameBufferAttachmentFormat::Stencil_Attachment:
			attach->AttachmentFormat = GL_STENCIL_ATTACHMENT;
			break;
		};

		AddAttachToVector(attach);

		uint32 dataType = attach->DataType;

		switch (attach->DataType)
		{
		case RenderBufferDataType::Depth:
		case RenderBufferDataType::Depth_Multisample:
#if defined(GLES2) || defined(GLES3)
			attach->DataType = GL_DEPTH_COMPONENT16;
#else
			attach->DataType = GL_DEPTH_COMPONENT;
#endif

#if !defined (GLES2)
			// Add RenderBuffer
			if (dataType == RenderBufferDataType::Depth)
			{
				GLCHECKER(glRenderbufferStorage(GL_RENDERBUFFER, attach->DataType, attach->Width, attach->Height));
			}
			else {
				GLCHECKER(glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, attach->DataType, attach->Width, attach->Height));
			}
#else
			GLCHECKER(glRenderbufferStorage(GL_RENDERBUFFER, attach->DataType, attach->Width, attach->Height));
#endif
			// Same code for both types
			GLCHECKER(glFramebufferRenderbuffer(GL_FRAMEBUFFER, attach->AttachmentFormat, GL_RENDERBUFFER, attach->rboID));
			GLCHECKER(glBindRenderbuffer(GL_RENDERBUFFER, 0));

			break;
			// case RenderBufferDataType::Stencil:
			//     attach->DataType = GL_STENCIL_COMPONENT;
			// break;
		case RenderBufferDataType::RGBA:
		case RenderBufferDataType::RGBA_Multisample:
		default:
			attach->DataType = GL_RGBA;

#if !defined (GLES2)
			// Add RenderBuffer
			if (dataType == RenderBufferDataType::RGBA)
			{
				GLCHECKER(glRenderbufferStorage(GL_RENDERBUFFER, attach->DataType, attach->Width, attach->Height));
			}
			else {
				GLCHECKER(glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, attach->DataType, attach->Width, attach->Height));
			}
#else
			GLCHECKER(glRenderbufferStorage(GL_RENDERBUFFER, attach->DataType, attach->Width, attach->Height));
#endif
			// Same code for both types
			GLCHECKER(glFramebufferRenderbuffer(GL_FRAMEBUFFER, attach->AttachmentFormat, GL_RENDERBUFFER, attach->rboID));
			GLCHECKER(glBindRenderbuffer(GL_RENDERBUFFER, 0));
			break;
		}

		CheckFBOStatus();

#if !defined(GLES2) && !defined(GLES3)

		if (!drawBuffers)
		{
			GLCHECKER(glDrawBuffer(GL_NONE));
			GLCHECKER(glReadBuffer(GL_NONE));
		}

		if (attachments.size() > 0 && drawBuffers)
		{
			std::vector<GLenum> BufferIDs;
			uint32 counter = 0;
			for (std::vector<FBOAttachment*>::iterator i = attachments.begin(); i != attachments.end(); i++)
			{
				if ((*i)->AttachmentFormatInternal >= FrameBufferAttachmentFormat::Color_Attachment0 && (*i)->AttachmentFormatInternal <= FrameBufferAttachmentFormat::Color_Attachment15) {
					BufferIDs.push_back(GL_COLOR_ATTACHMENT0 + counter++);
				}
			}

			GLCHECKER(glDrawBuffers(BufferIDs.size(), &BufferIDs[0]));
		}

#endif

		if (!isBinded)
			GLCHECKER(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	}
	void FrameBuffer::AddAttachToVector(FBOAttachment* attach)
	{
		for (std::vector<FBOAttachment*>::iterator i = attachments.begin(); i != attachments.end(); i++)
		{
			if ((*i)->AttachmentFormat == attach->AttachmentFormat)
			{
				delete *i;
				*i = attach;
				return;
			}
		}
		attachments.push_back(attach);
	}
	void FrameBuffer::Bind(const uint32 access)
	{
		// bind fbo
		glAccessBinded = GL_FRAMEBUFFER;
		switch (access)
		{
#if !defined(GLES2)
		case FBOAccess::Read:
			glAccessBinded = GL_READ_FRAMEBUFFER;
			break;
		case FBOAccess::Write:
			glAccessBinded = GL_DRAW_FRAMEBUFFER;
			break;
#endif
		default:
			glAccessBinded = GL_FRAMEBUFFER;
			break;
		}

		// Keep type of binded access
		accessBinded = access;

		// Add to bound FBOs
		BoundFBOs[accessBinded].push_back(this);

		GLCHECKER(glBindFramebuffer(glAccessBinded, fbo));
		isBinded = true;
	}
	uint32 FrameBuffer::GetBindID()
	{
		return fbo;
	}
	void FrameBuffer::UnBind()
	{
		// unbind fbo
		GLCHECKER(glBindFramebuffer(glAccessBinded, 0));

#if !defined(GLES2) && !defined(GLES3)
		if (drawBuffers)
		{
			if (glAccessBinded == GL_DRAW_FRAMEBUFFER || glAccessBinded == GL_FRAMEBUFFER)
				GLCHECKER(glDrawBuffer(GL_BACK));
			if (glAccessBinded == GL_READ_BUFFER || glAccessBinded == GL_FRAMEBUFFER)
				GLCHECKER(glReadBuffer(GL_BACK));
		}
#endif

		for (std::vector<FBOAttachment*>::iterator i = attachments.begin(); i != attachments.end(); i++)
		{
			if ((*i)->AttachmentType == FBOAttachmentType::Texture)
			{
				(*i)->TexturePTR->UpdateMipmap();
			}
		}

		isBinded = false;

		// Bind next FBO
		for (int32 i = BoundFBOs[accessBinded].size()-1; i >= 0; i--)
		{
			if (BoundFBOs[accessBinded][i] == this) {
				BoundFBOs[accessBinded].erase(BoundFBOs[accessBinded].begin() + i);
				i--;
			}
			else {
				GLCHECKER(glBindFramebuffer(BoundFBOs[accessBinded][i]->glAccessBinded, BoundFBOs[accessBinded][i]->fbo));
				break;
			}
		}
		// End Binding next FBO
	}
	bool FrameBuffer::IsBinded()
	{
		return isBinded;
	}
	void FrameBuffer::Resize(const uint32 Width, const uint32 Height)
	{
		for (std::vector<FBOAttachment*>::iterator i = attachments.begin(); i != attachments.end(); i++)
		{
			if ((*i)->AttachmentType == FBOAttachmentType::RenderBuffer)
			{
				GLCHECKER(glBindRenderbuffer(GL_RENDERBUFFER, (*i)->rboID));
				GLCHECKER(glRenderbufferStorage(GL_RENDERBUFFER, (*i)->DataType, Width, Height));
				GLCHECKER(glBindRenderbuffer(GL_RENDERBUFFER, 0));
			}
			else
			{
				(*i)->TexturePTR->Resize(Width, Height);
			}
		}
	}
	const uint32 &FrameBuffer::GetFrameBufferFormat() const
	{
		return framebufferFormat;
	}
}