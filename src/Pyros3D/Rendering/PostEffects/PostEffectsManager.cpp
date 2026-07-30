//============================================================================
// Name        : PostEffectsManager.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Post Effects Manager
//============================================================================

#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>

namespace p3d {

	// Same reasoning as IRenderer's ResolveInitialDevice() (IRenderer.cpp) -
	// borrow the already-active device (the main ForwardRenderer/
	// DeferredRenderer is always constructed first) instead of always
	// creating a second, owned GLRenderDevice, which crashed instantly on
	// a Vulkan-only build (no real GL context, every glad function
	// pointer NULL).
	static MaybeOwningDevicePtr ResolvePostEffectsDevice()
	{
		if (IsActiveRenderDeviceSet())
			return MaybeOwningDevicePtr(&GetActiveRenderDevice(), MaybeOwningDeviceDeleter{false});
		return MaybeOwningDevicePtr(new GLRenderDevice(), MaybeOwningDeviceDeleter{true});
	}

	PostEffectsManager::PostEffectsManager(const uint32 width, const uint32 height) : device(ResolvePostEffectsDevice())
	{
		// Save Dimensions
		Width = width;
		Height = height;

		Color = new Texture();
		// RGBA16F, not RGBA8 - this is what every wrapped RenderScene()
		// call (Forward or Deferred) draws into. Additive multi-light PBR
		// accumulation routinely exceeds 1.0 per channel; an 8-bit unorm
		// target would hard-clip those values here, before TonemapEffect
		// (see Effects/TonemapEffect.h) ever gets a chance to roll them
		// off gracefully. Both backends' format tables already handle
		// this format fully (GLRenderDevice/VulkanRenderDevice
		// TranslateTextureFormat()) - no backend work needed.
		Color->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height, false);
		Color->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		Depth = new Texture();
		Depth->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
		Depth->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		// Initialize Internal FBO
		ExternalFBO = new FrameBuffer();
		ExternalFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, Depth);
		ExternalFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, Color);
	}

	FrameBuffer* PostEffectsManager::GetExternalFrameBuffer()
	{
		return ExternalFBO;
	}

	void PostEffectsManager::CaptureFrame()
	{
		ExternalFBO->Bind();
	}
	void PostEffectsManager::EndCapture()
	{
		ExternalFBO->UnBind();
	}

	void PostEffectsManager::Resize(const uint32 width, const uint32 height)
	{
		// Save Dimensions
		Width = width;
		Height = height;

		// Resize External FBO
		ExternalFBO->Resize(Width, Height);
	}

	void PostEffectsManager::ProcessPostEffects(Projection* projection)
	{
		// Set Counter
		uint32 counter = 1;

		// Save Near and Far Planes
		Vec2 NearFarPlane = Vec2(projection->Near, projection->Far);
		Vec2 ScreenDimensions = Vec2((f32)Width, (f32)Height);

		// Run Through Effects
		for (std::vector<IEffect*>::iterator effect = effects.begin(); effect != effects.end(); effect++)
		{
			if (counter == effects.size())
			{
				// Last effect draws to the swapchain, not an offscreen
				// FBO - a no-op on GL (no acquire/present step), but on
				// Vulkan nothing else in this class's callers ever
				// acquires+presents a swapchain frame: every example
				// using PostEffectsManager wraps its *only* RenderScene()
				// call in CaptureFrame()/EndCapture() (an offscreen FBO),
				// so RenderScene()'s own isMainSwapchainPass gate always
				// skips BeginFrame()/EndFrame() there. Without this call,
				// the swapchain is never acquired for the whole frame -
				// found via every PostEffectsManager-based example
				// (SSAOExample/DepthOfField/MotionBlurExample) hanging
				// forever in vkWaitForFences() on this loop's *next*
				// offscreen FBO fence (or, for a single-effect chain,
				// this exact draw's own eventual submit) - a real GPU
				// completion signal that, per MoltenVK's own behavior on
				// this machine, never arrives without an actual
				// swapchain present somewhere in the frame to drive it.
				device->BeginFrame();
				device->SetViewport(0, 0, Width, Height);
			}
			else {

				activeFBO = (*effect)->fbo;

				device->SetViewport(0, 0, (*effect)->Width, (*effect)->Height);

				// Bind FBO
				activeFBO->Bind();
			}

			// Clear Screen
			device->SetClearColor(Vec4(0.f, 0.f, 0.f, 0.f));
			device->Clear(device->TranslateBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth));

			DeviceHandle vao = device->CreateVertexArray();
			CommandBufferHandle cmd = device->BeginCommandBuffer();
			device->BindVertexArray(cmd, vao);

			// Start Shader Program
			device->UseProgram((*effect)->shader->ShaderProgram());

			// Bind this effect's pipeline (built lazily, once - see
			// IEffect.h's comment on pipelineHandle) - PostEffectsManager
			// previously only ever called UseProgram(), which sets
			// currentProgram but never currentPipeline, so DrawArrays()
			// below always found no pipeline bound and silently drew
			// nothing on Vulkan (GL has no pipeline-object concept, so
			// this was invisible there). Must happen *before* the sampler
			// texture-unit binds below (SendUniformInt() writes into
			// pipelineSamplerSets[currentPipeline] - see its comment -
			// so currentPipeline has to already be this effect's own
			// pipeline, not still whatever the previous effect/mesh left
			// it as, or the descriptor write lands on the wrong pipeline
			// and this one's sampler descriptors are never updated at
			// all - VUID-vkCmdDraw-None-08114 the moment it draws).
			if ((*effect)->pipelineHandle == 0)
			{
				IRenderDevice::PipelineDesc pdesc;
				pdesc.shaderProgram = (*effect)->shader->ShaderProgram();
				pdesc.depthTest = false;
				pdesc.depthWrite = false;
				pdesc.blendingEnabled = false;
				pdesc.cullFace = CullFace::DoubleSided;
				pdesc.noVertexInput = true;
				(*effect)->pipelineHandle = device->CreatePipeline(pdesc);
			}
			device->BindPipeline(cmd, (*effect)->pipelineHandle);

			// Bind MRT
			for (std::vector<RTT::Info>::iterator i = (*effect)->RTTOrder.begin(); i != (*effect)->RTTOrder.end(); i++)
			{
				switch ((*i).Type)
				{
				case RTT::Color:
					Color->Bind();
					break;
				case RTT::Depth:
					Depth->Bind();
					break;
				case RTT::LastRTT:
					LastRTT->Bind();
					break;
				default:
					(*i).texture->Bind();
					break;
				}
			}

			// Send Uniforms
			for (std::list<__UniformPostProcess>::iterator i = (*effect)->Uniforms.begin(); i != (*effect)->Uniforms.end(); i++)
			{
				if ((*i).handle == -2)
				{
					(*i).handle = Shader::GetUniformLocation((*effect)->shader->ShaderProgram(), (*i).uniform.Name);
				}

				// Resolve this uniform's current value + byte size,
				// regardless of handle - GetUniformLocation() only ever
				// resolves *sampler* names on Vulkan (see its comment in
				// VulkanRenderDevice.cpp), so every scalar/vector/matrix
				// uniform here always has handle==-1 there; the extras
				// UBO path below (extraUniformOffsets) is how those
				// actually reach the shader on that backend instead.
				const void* valuePtr = NULL;
				uint32 valueSize = 0;
				switch ((*i).uniform.Usage)
				{
				case PostEffects::NearFarPlane:
					(*i).uniform.Type = Uniforms::DataType::Vec2;
					valuePtr = &NearFarPlane; valueSize = sizeof(NearFarPlane);
					break;
				case PostEffects::ScreenDimensions:
					(*i).uniform.Type = Uniforms::DataType::Vec2;
					valuePtr = &ScreenDimensions; valueSize = sizeof(ScreenDimensions);
					break;
				case PostEffects::ProjectionFromScene:
					(*i).uniform.Type = Uniforms::DataType::Matrix;
					valuePtr = &projection->m; valueSize = sizeof(projection->m);
					break;
				default:
				case PostEffects::Other:
					valuePtr = (*i).uniform.Value.empty() ? NULL : &(*i).uniform.Value[0];
					valueSize = (uint32)(*i).uniform.Value.size();
					break;
				}

				if (!(*effect)->extraUniformOffsets.empty() && valuePtr != NULL)
				{
					std::map<std::string, uint32>::const_iterator offIt = (*effect)->extraUniformOffsets.find((*i).uniform.Name);
					if (offIt != (*effect)->extraUniformOffsets.end() && offIt->second + valueSize <= (*effect)->extraUniformsScratch.size())
						memcpy(&(*effect)->extraUniformsScratch[offIt->second], valuePtr, valueSize);
				}

				if ((*i).handle != -1)
				{
					switch ((*i).uniform.Usage)
					{
					case PostEffects::NearFarPlane:
						Shader::SendUniform((*i).uniform, &NearFarPlane, (*i).handle);
						break;
					case PostEffects::ScreenDimensions:
						Shader::SendUniform((*i).uniform, &ScreenDimensions, (*i).handle);
						break;
					case PostEffects::ProjectionFromScene:
						Shader::SendUniform((*i).uniform, &projection->m, (*i).handle);
						break;
					default:
					case PostEffects::Other:
						Shader::SendUniform((*i).uniform, (*i).handle);
						break;
					}
				}
			}

			// Deliver this effect's non-sampler uniforms (packed above)
			// via a real UBO - see IEffect.h's comment on
			// extraUniformsBinding. No-op for an effect with none
			// (extraUniformsBinding stays 0 - most GL-only-tested effects
			// with no non-sampler uniforms, e.g. SSAOEffectFinal, never
			// touch this). Created lazily since it needs a linked program
			// (for BindUniformBlockIfPresent()'s reflectedBindings check)
			// to already exist, which CompileShaders() guarantees by now.
			if ((*effect)->extraUniformsBinding != 0)
			{
				if ((*effect)->extraUniformsBufferHandle == 0)
					(*effect)->extraUniformsBufferHandle = device->CreateUniformBuffer((*effect)->extraUniformsSize, (*effect)->extraUniformsBinding);
				device->BindUniformBlockIfPresent((*effect)->shader->ShaderProgram(), (*effect)->extraUniformsBlockName, (*effect)->extraUniformsBinding);
				device->ReplaceUniformBuffer((*effect)->extraUniformsBufferHandle, (*effect)->extraUniformsSize, &(*effect)->extraUniformsScratch[0]);
			}

			device->DrawArrays(device->TranslateDrawType(DrawingType::Triangles), 0, 3);
			device->EndCommandBuffer(cmd);

			// Unbind MRT
			for (std::vector<RTT::Info>::reverse_iterator i = (*effect)->RTTOrder.rbegin(); i != (*effect)->RTTOrder.rend(); i++)
			{
				switch ((*i).Type)
				{
				case RTT::Color:
					Color->Unbind();
					break;
				case RTT::Depth:
					Depth->Unbind();
					break;
				case RTT::LastRTT:
					LastRTT->Unbind();
					break;
				default:
					(*i).texture->Unbind();
					break;
				}
			}

			// Unbind FBO if is using and set the RTT
			if (counter < effects.size())
			{
				activeFBO->UnBind();
				// Get RTT
				LastRTT = activeFBO->GetAttachments()[0]->TexturePTR;
			}

			// count loop
			counter++;

		}

		// Disable Shader Program
		device->UseProgram(0);

		// Matches the BeginFrame() call above - this is the last
		// rendering call of the frame for every example that uses
		// PostEffectsManager (nothing else calls RenderScene() again
		// after this), so this is where the swapchain frame this class
		// started actually gets submitted+presented. No-op on GL
		// (SDL_GL_SwapWindow() happens unconditionally elsewhere,
		// unaffected by this); guarded by frameInProgress on Vulkan, so
		// harmless if BeginFrame() above was itself a no-op (already in
		// progress from something else).
		device->EndFrame();
	}

	PostEffectsManager::~PostEffectsManager()
	{
		for (std::vector<IEffect*>::iterator i = effects.begin(); i != effects.end(); i++)
		{
			delete (*i);
		}

		delete ExternalFBO;

		// Destroy Textures
		delete Color;
		delete Depth;
	}

	void PostEffectsManager::AddEffect(IEffect* Effect)
	{
		// Add New Effect
		effects.push_back(Effect);
	}
	void PostEffectsManager::RemoveEffect(IEffect* Effect)
	{
		for (std::vector<IEffect*>::iterator i = effects.begin(); i != effects.end(); i++)
		{
			if ((*i) == Effect)
			{
				effects.erase(i);
				break;
			}
		}
	}

	const uint32 PostEffectsManager::GetNumberEffects() const
	{
		return effects.size();
	}

}
