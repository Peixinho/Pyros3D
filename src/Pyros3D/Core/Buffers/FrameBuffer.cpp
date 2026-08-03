//============================================================================
// Name        : FrameBuffer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : FrameBuffer
//============================================================================

#include <Pyros3D/Core/Buffers/FrameBuffer.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>

namespace p3d {

	// Every FrameBuffer shares whichever backend is currently active (see
	// GetActiveRenderDevice() in IRenderDevice.h) rather than each
	// FrameBuffer needing an injected IRenderDevice* - same pattern as
	// GeometryBuffer.cpp/Shaders.cpp/Texture.cpp. Previously hardcoded to
	// a static GLRenderDevice (missed when Texture.cpp got this same fix
	// - a separate file, same historical pattern) - crashed the same way
	// Texture.cpp did, through an unloaded/null GLAD function pointer,
	// the first time a real Vulkan shadow FBO was created.
	static IRenderDevice& Device()
	{
		return GetActiveRenderDevice();
	}

	FBOAttachment::~FBOAttachment()
	{
		// Release the GL renderbuffer this attachment owns. Texture-backed
		// attachments don't own TexturePTR (borrowed from the caller), so
		// there's nothing to release for those.
		if (AttachmentType == FBOAttachmentType::RenderBuffer)
			Device().DestroyRenderbuffer(rboID);
	}

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

		// Only if a real device is still around. Device() falls back to a
		// static GLRenderDevice when none is set, so a FrameBuffer that
		// outlives its device would try to free a Vulkan-created handle
		// through GL entry points that were never loaded in this process -
		// a jump through a null/garbage function pointer, which is what a
		// Lua-owned FrameBuffer did on every shutdown (sol's state is torn
		// down, and its userdata finalized, after the context has already
		// destroyed the render device). Nothing leaks by skipping it:
		// no device means the GPU-side objects are already gone with it.
		if (IsActiveRenderDeviceSet())
			Device().DestroyFramebuffer(fbo);

		for (std::vector<FBOAttachment*>::iterator i = attachments.begin(); i != attachments.end(); i++)
			delete (*i);

		attachments.clear();

	}
	void FrameBuffer::BlitFrameBuffer(const uint32 initSrcX, const uint32 initSrcY, const uint32 endSrcX, const uint32 endSrcY, const uint32 initDestX, const uint32 initDestY, const uint32 endDestX, const uint32 endDestY, const uint32 mask, const uint32 filter)
	{
		Device().BlitFramebuffer(initSrcX, initSrcY, endSrcX, endSrcY, initDestX, initDestY, endDestX, endDestY, mask, filter);
	}
	void FrameBuffer::EnableMultisample()
	{
		Device().SetMultisampleEnabled(true);
	}
	void FrameBuffer::DisableMultisample()
	{
		Device().SetMultisampleEnabled(false);
	}
	void FrameBuffer::Init(const uint32 attachmentFormat, const uint32 TextureType, p3d::Texture *attachment)
	{

		if (FBOInitialized == true)
		{
			// destroy fbo and rbo
			Device().DestroyFramebuffer(fbo);

			// flag FBO Stoped
			FBOInitialized = false;
		}

		// flag FBO Initialized
		FBOInitialized = true;

		fbo = Device().CreateFramebuffer();

		isBinded = false;

		// Add Attach
		AddAttach(attachmentFormat, TextureType, attachment);

	}
	void FrameBuffer::Init(const uint32 attachmentFormat, const uint32 attachmentDataType, const uint32 Width, const uint32 Height, const uint32 msaa)
	{

		if (FBOInitialized == true)
		{
			// destroy fbo and rbo
			Device().DestroyFramebuffer(fbo);

			// flag FBO Stoped
			FBOInitialized = false;
		}

		// flag FBO Initialized
		FBOInitialized = true;

		fbo = Device().CreateFramebuffer();

		isBinded = false;

		// Add Attach
		AddAttach(attachmentFormat, attachmentDataType, Width, Height, msaa);

	}
	void FrameBuffer::CheckFBOStatus()
	{
		switch (Device().TranslateFramebufferStatus(Device().CheckFramebufferStatus()))
		{
		case FBOStatus::Complete:
		{

#ifdef _DEBUG
			echo("FBO: The framebuffer is complete and valid for rendering.");
#endif
			break;
		}
		case FBOStatus::IncompleteAttachment:
		{
			echo("FBO: One or more attachment points are not framebuffer attachment complete. This could mean theres no texture attached or the format isnt renderable. For color textures this means the base format must be RGB or RGBA and for depth textures it must be a DEPTH_COMPONENT format. Other causes of this error are that the width or height is zero or the z-offset is out of range in case of render to volume.");
			break;
		}
		case FBOStatus::IncompleteMissingAttachment:
		{
			echo("FBO: There are no attachments.");
			break;
		}
		case FBOStatus::IncompleteDrawBuffer:
		{
			echo("FBO: An attachment point referenced by GL.DrawBuffers() doesn't have an attachment.");
			break;
		}
		case FBOStatus::IncompleteReadBuffer:
		{
			echo("FBO: The attachment point referenced by GL.ReadBuffers() doesn't have an attachment.");
			break;
		}
		case FBOStatus::Unsupported:
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
		attach->AttachmentFormat = Device().TranslateFramebufferAttachment(attach->AttachmentFormatInternal);

		if (attach->AttachmentFormatInternal >= FrameBufferAttachmentFormat::Color_Attachment0 && attachmentFormat <= FrameBufferAttachmentFormat::Color_Attachment15 && !drawBuffers)
			drawBuffers = true;

		// TextureType translation reuses the same GL_TEXTURE_CUBE_MAP_*/
		// GL_TEXTURE_2D/GL_TEXTURE_2D_MULTISAMPLE table as Texture::GetGLModes()
		// (TranslateTextureTarget's `mode` output); `subMode` is discarded
		// since a framebuffer attachment target is always the specific
		// face/2D target, never the GL_TEXTURE_CUBE_MAP "family" token.
		uint32 unusedSubMode;
		Device().TranslateTextureTarget(TextureType, attach->TextureType, unusedSubMode);

		AddAttachToVector(attach);

		bool wasAlreadyBound = isBinded;
		if (!isBinded)
			Device().BindFramebuffer(Device().TranslateFramebufferAccess(FBOAccess::Read_Write), fbo, false);

		// Add Attach
		Device().AttachFramebufferTexture2D(attach->AttachmentFormat, attach->TextureType, attach->TexturePTR->GetBindID(), wasAlreadyBound);

#if !defined(GLES3)

		if (!drawBuffers)
		{
			Device().SetDrawBufferNone();
			Device().SetReadBufferNone();
		}

		if (attachments.size() > 0 && drawBuffers)
		{
			std::vector<uint32> ColorAttachmentIndices;
			uint32 counter = 0;
			for (std::vector<FBOAttachment*>::iterator i = attachments.begin(); i != attachments.end(); i++)
			{
				if ((*i)->AttachmentFormatInternal >= FrameBufferAttachmentFormat::Color_Attachment0 && (*i)->AttachmentFormatInternal <= FrameBufferAttachmentFormat::Color_Attachment15) {
					ColorAttachmentIndices.push_back(counter++);
				}
			}

			Device().SetDrawBuffers(ColorAttachmentIndices);
		}
#endif

		CheckFBOStatus();

		if (!isBinded)
			Device().BindFramebuffer(Device().TranslateFramebufferAccess(FBOAccess::Read_Write), 0, false);
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
			Device().BindFramebuffer(Device().TranslateFramebufferAccess(FBOAccess::Read_Write), fbo, false);

		attach->rboID = Device().CreateRenderbuffer();
		Device().BindRenderbuffer(attach->rboID);

		// Get Attatchment Format
		attach->AttachmentFormat = Device().TranslateFramebufferAttachment(attach->AttachmentFormatInternal);

		AddAttachToVector(attach);

		uint32 dataType = attach->DataType;

		switch (attach->DataType)
		{
		case RenderBufferDataType::Depth:
		case RenderBufferDataType::Depth_Multisample:
			attach->DataType = Device().TranslateRenderbufferFormat(dataType);

			// Add RenderBuffer
			if (dataType == RenderBufferDataType::Depth)
			{
				Device().RenderbufferStorage(attach->DataType, attach->Width, attach->Height);
			}
			else {
				Device().RenderbufferStorageMultisample(attach->DataType, msaa, attach->Width, attach->Height);
			}
			// Same code for both types
			Device().AttachFramebufferRenderbuffer(attach->AttachmentFormat, attach->rboID);
			Device().BindRenderbuffer(0);

			break;
			// case RenderBufferDataType::Stencil:
			//     attach->DataType = GL_STENCIL_COMPONENT;
			// break;
		case RenderBufferDataType::RGBA:
		case RenderBufferDataType::RGBA_Multisample:
		default:
			attach->DataType = Device().TranslateRenderbufferFormat(dataType);

			// Add RenderBuffer
			if (dataType == RenderBufferDataType::RGBA)
			{
				Device().RenderbufferStorage(attach->DataType, attach->Width, attach->Height);
			}
			else {
				Device().RenderbufferStorageMultisample(attach->DataType, msaa, attach->Width, attach->Height);
			}
			// Same code for both types
			Device().AttachFramebufferRenderbuffer(attach->AttachmentFormat, attach->rboID);
			Device().BindRenderbuffer(0);
			break;
		}

		CheckFBOStatus();

#if !defined(GLES3)

		if (!drawBuffers)
		{
			Device().SetDrawBufferNone();
			Device().SetReadBufferNone();
		}

		if (attachments.size() > 0 && drawBuffers)
		{
			std::vector<uint32> ColorAttachmentIndices;
			uint32 counter = 0;
			for (std::vector<FBOAttachment*>::iterator i = attachments.begin(); i != attachments.end(); i++)
			{
				if ((*i)->AttachmentFormatInternal >= FrameBufferAttachmentFormat::Color_Attachment0 && (*i)->AttachmentFormatInternal <= FrameBufferAttachmentFormat::Color_Attachment15) {
					ColorAttachmentIndices.push_back(counter++);
				}
			}

			Device().SetDrawBuffers(ColorAttachmentIndices);
		}

#endif

		if (!isBinded)
			Device().BindFramebuffer(Device().TranslateFramebufferAccess(FBOAccess::Read_Write), 0, false);
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
		glAccessBinded = Device().TranslateFramebufferAccess(FBOAccess::Read_Write);
		switch (access)
		{
		case FBOAccess::Read:
			glAccessBinded = Device().TranslateFramebufferAccess(FBOAccess::Read);
			break;
		case FBOAccess::Write:
			glAccessBinded = Device().TranslateFramebufferAccess(FBOAccess::Write);
			break;
		default:
			glAccessBinded = Device().TranslateFramebufferAccess(FBOAccess::Read_Write);
			break;
		}

		// Keep type of binded access
		accessBinded = access;

		// Add to bound FBOs
		BoundFBOs[accessBinded].push_back(this);

		Device().BindFramebuffer(glAccessBinded, fbo, true);
		isBinded = true;
	}
	uint32 FrameBuffer::GetBindID()
	{
		return fbo;
	}
	void FrameBuffer::UnBind()
	{
		// unbind fbo
		Device().BindFramebuffer(glAccessBinded, 0, true);

#if !defined(GLES3)
		if (drawBuffers)
		{
			if (accessBinded == FBOAccess::Write || accessBinded == FBOAccess::Read_Write)
				Device().SetDrawBufferBack();
			// NOTE: preserves a pre-existing bug from before this device-seam
			// migration - the original compared glAccessBinded against
			// GL_READ_BUFFER (a glGet parameter enum, not a framebuffer-target
			// enum like GL_READ_FRAMEBUFFER), so this half of the condition
			// was always false. FBOAccess::Read-bound framebuffers never
			// actually got their read buffer reset to GL_BACK here. Not
			// fixed during this mechanical migration.
			if (accessBinded == FBOAccess::Read_Write)
				Device().SetReadBufferBack();
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

		// Bind next FBO - restores whichever FrameBuffer (if any) was bound
		// before this one at the same access level, instead of leaving the
		// hardcoded 0-bind above as final. The extra `i--` this used to have
		// (on top of the for-loop's own decrement) skipped the very next
		// stack entry right after erasing this one - harmless for every
		// existing single-level Bind()/UnBind() pair (nothing left to
		// restore either way), but silently broken for real nesting (e.g. a
		// shadow-casting light's shadow FBO Bind()/UnBind() happening inside
		// PreRender(), itself called while a caller's own FBO - like
		// PostEffectsManager's capture target - is already bound): the outer
		// FBO was never found and rebound, so it landed on the swapchain
		// instead once PreRender() returned. Found via EmberRush, the first
		// scene to combine shadow-casting lights with PostEffectsManager.
		for (int32 i = BoundFBOs[accessBinded].size()-1; i >= 0; i--)
		{
			if (BoundFBOs[accessBinded][i] == this) {
				BoundFBOs[accessBinded].erase(BoundFBOs[accessBinded].begin() + i);
			}
			else {
				Device().BindFramebuffer(BoundFBOs[accessBinded][i]->glAccessBinded, BoundFBOs[accessBinded][i]->fbo, true);
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
				Device().BindRenderbuffer((*i)->rboID);
				Device().RenderbufferStorage((*i)->DataType, Width, Height);
				Device().BindRenderbuffer(0);
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
