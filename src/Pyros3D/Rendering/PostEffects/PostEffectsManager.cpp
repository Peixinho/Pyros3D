//============================================================================
// Name        : PostEffectsManager.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Post Effects Manager
//============================================================================

#include <Pyros3D/Rendering/PostEffects/PostEffectsManager.h>
#include <Pyros3D/Rendering/PostEffects/Effects/GammaEncodeEffect.h>
#include <Pyros3D/Rendering/Device/GLRenderDevice.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>

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
			return BorrowActiveRenderDevice();
		// Not published as active - see DebugRenderer's identical comment.
		return std::make_shared<GLRenderDevice>();
	}

	PostEffectsManager::PostEffectsManager(const uint32 width, const uint32 height) : device(ResolvePostEffectsDevice()), fullscreenVao(0), viewportGammaEffect(NULL)
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
		// Nearest, not CreateEmptyTexture's default Linear. Apple's GL calls
		// a Linear-filtered depth texture "unloadable" and silently
		// substitutes the zero texture for it - the same trap the shadow
		// maps document in DirectionalLight::EnableCastShadows(). Every
		// effect that reads RTT::Depth (SSAO, depth of field, motion blur)
		// then samples zeros and produces nothing, with no error anywhere
		// except one driver log line. No compare mode here, unlike a shadow
		// map: this is read as a plain sampler2D. Filtering depth is
		// meaningless anyway - a blend of two depths is a surface that is
		// not there - so Nearest is what these effects want regardless.
		Depth->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);

		// Initialize Internal FBO
		ExternalFBO = new FrameBuffer();
		ExternalFBO->SetDebugName("Post effects capture");
		ExternalFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, Depth);
		ExternalFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, Color);

		fullscreenVao = device->CreateVertexArray();
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
		if (width == 0 || height == 0)
			return;
		if (width == Width && height == Height)
			return;

		// Save Dimensions
		Width = width;
		Height = height;

		// Resize External FBO
		ExternalFBO->Resize(Width, Height);

		if (viewportGammaEffect != NULL)
			viewportGammaEffect->Resize(Width, Height);

		// The chain too, not just the capture. An effect keeps whatever size
		// it was built with, so after a viewport resize the last effect's
		// texture no longer matches the frame it is displayed in and the
		// image is resampled to fit - the scene visibly shifts and rescales
		// the moment a chain is added. Every effect reads full-screen
		// texcoords, so they all want the target's size; the only ones that
		// deliberately differ would be half-res stages, and there are none
		// here.
		for (std::vector<IEffect*>::iterator i = effects.begin(); i != effects.end(); i++)
			(*i)->Resize(Width, Height);
	}

	void PostEffectsManager::EnsureViewportGammaEffect()
	{
		if (viewportGammaEffect != NULL)
			return;
		// Gamma only - not TonemapEffect. ACES + encode made the editor
		// viewport brighter than OpenGL's linear ImGui display of the same
		// RGBA16F capture; pow(1/2.2) is enough to undo UNORM-as-sRGB
		// darkening without the filmic lift.
		viewportGammaEffect = new GammaEncodeEffect(RTT::Color, Width, Height);
	}

	void PostEffectsManager::BlitViewportGamma()
	{
		EnsureViewportGammaEffect();
		IEffect *effect = viewportGammaEffect;

		// Do not clobber the scene clear colour with transparent black
		// before binding - CaptureFrame()'s next Bind uses pendingClearColor.
		// The display FBO is fully overwritten by the blit either way.
		activeFBO = effect->fbo;
		activeFBO->Bind();
		device->SetViewport(0, 0, effect->Width, effect->Height);

		CommandBufferHandle cmd = device->BeginCommandBuffer();
		device->BindVertexArray(cmd, fullscreenVao);
		device->UseProgram(effect->shader->ShaderProgram());

		if (effect->pipelineHandle == 0)
		{
			IRenderDevice::PipelineDesc pdesc;
			pdesc.shaderProgram = effect->shader->ShaderProgram();
			pdesc.depthTest = false;
			pdesc.depthWrite = false;
			pdesc.blendingEnabled = false;
			pdesc.cullFace = CullFace::DoubleSided;
			pdesc.noVertexInput = true;
			effect->pipelineHandle = device->CreatePipeline(pdesc);
			effect->pipelineBuiltForSwapchainGeneration = device->GetSwapchainGeneration();
			if (effect->pipelineHandle == 0)
			{
				fprintf(stderr, "PostEffectsManager::BlitViewportGamma: CreatePipeline failed - viewport will stay linear/dark\n");
				activeFBO->UnBind();
				return;
			}
		}
		// BindPipeline before sampler SendUniformInt - same ordering
		// ProcessPostEffects requires (writes go to pipelineSamplerSets).
		device->BindPipeline(cmd, effect->pipelineHandle);

		Texture::ResetUnitCounter();
		for (std::vector<RTT::Info>::iterator i = effect->RTTOrder.begin(); i != effect->RTTOrder.end(); i++)
		{
			switch ((*i).Type)
			{
			case RTT::Color: Color->Bind(); break;
			case RTT::Depth: Depth->Bind(); break;
			case RTT::LastRTT: if (LastRTT) LastRTT->Bind(); break;
			default: if ((*i).texture) (*i).texture->Bind(); break;
			}
		}

		// Deliver sampler unit ints the same way ProcessPostEffects does -
		// a hard-coded SendUniformInt(0) misses Vulkan/Metal reflection
		// when the binding id is not 0 or the name fails to resolve.
		for (std::list<__UniformPostProcess>::iterator i = effect->Uniforms.begin(); i != effect->Uniforms.end(); i++)
		{
			if ((*i).handle == -2)
				(*i).handle = Shader::GetUniformLocation(effect->shader->ShaderProgram(), (*i).uniform.Name);
			if ((*i).handle != -1)
				Shader::SendUniform((*i).uniform, (*i).handle);
		}

		device->DrawArrays(device->TranslateDrawType(DrawingType::Triangles), 0, 3);
		device->EndCommandBuffer(cmd);

		for (std::vector<RTT::Info>::reverse_iterator i = effect->RTTOrder.rbegin(); i != effect->RTTOrder.rend(); i++)
		{
			switch ((*i).Type)
			{
			case RTT::Color: Color->Unbind(); break;
			case RTT::Depth: Depth->Unbind(); break;
			case RTT::LastRTT: if (LastRTT) LastRTT->Unbind(); break;
			default: if ((*i).texture) (*i).texture->Unbind(); break;
			}
		}
		activeFBO->UnBind();
		device->UseProgram(0);
	}

	void PostEffectsManager::SetViewMatrix(const Matrix &view)
	{
		viewMatrix = view;
		viewMatrixInverse = view.Inverse();
		haveViewMatrix = true;
	}

	Texture* PostEffectsManager::GetFinalTexture()
	{
		// Falls back to the capture on purpose - a caller showing "the result"
		// should not have to branch on whether a chain exists, and with no chain
		// the result IS the captured frame.
		if (renderLastToTexture && finalTexture != NULL && !effects.empty())
			return finalTexture;
		return Color;
	}

	Texture* PostEffectsManager::GetViewportColor()
	{
		// Editor ImGui path: show the linear RGBA16F capture as-is, same as
		// OpenGL. A pow(1/2.2) blit made VK/Metal midtones brighter than GL
		// (e.g. clear 0.2 → ~0.48). Swapchain UNORM darkening is a present
		// issue for demos that draw to the backbuffer; this texture is only
		// sampled by ImGui, which already matched GL once the capture clear
		// colour was applied before Bind.
		return Color;
	}

	void PostEffectsManager::ProcessPostEffects(Projection* projection)
	{
		if (effects.empty())
			return;

		// Save Near and Far Planes
		Vec2 NearFarPlane = Vec2(projection->Near, projection->Far);
		Vec2 ScreenDimensions = Vec2((f32)Width, (f32)Height);

		// Post-effect RTT binds start at unit 0; a prior RenderScene that
		// leaked UnitBinded would make uTex0/uTex1 point at the wrong units
		// while the uniforms still say 0/1 (sharp colour, zero velocity).
		Texture::ResetUnitCounter();

		// "The last render target", before any effect has run, is the frame
		// the scene was captured into. Without this LastRTT is still NULL for
		// the FIRST effect of the chain, and the binding switch below
		// dereferences it - so any chain whose first effect read LastRTT
		// crashed instead of running. It never came up while every chain was
		// built by hand in Lua, where the first effect is always written to
		// read Color; it is the first thing that happens when the chain comes
		// from a scene file, since "read whatever came before me" is the
		// obvious thing for an effect to ask for and the only thing an
		// author-supplied effect can portably ask for.
		Texture* const sceneFrame = (sceneSource != NULL) ? sceneSource : Color;
		LastRTT = sceneFrame;

		// Each effect pass below clears its own target to transparent
		// black, which overwrites the device-wide clear colour the *scene*
		// wants. On GL that was invisible: RenderScene() issues its own
		// glClear() after DrawBackground() has re-applied the colour. On
		// Vulkan/Metal the scene's clear happens implicitly when
		// CaptureFrame() binds ExternalFBO - i.e. *before* RenderScene()
		// runs at all - so it used whatever this loop left behind on the
		// previous frame, and any scene with both a background and a post
		// chain rendered on black instead (DepthOfField's red background,
		// most visibly). Put back what we found once the chain is done.
		const Vec4 sceneClearColor = device->GetClearColor();

		auto drawEffect = [&](IEffect *effect, const bool isLastEffect)
		{
			// Clear Screen
			device->SetClearColor(Vec4(0.f, 0.f, 0.f, 0.f));
			device->Clear(device->TranslateBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth));

			CommandBufferHandle cmd = device->BeginCommandBuffer();
			device->BindVertexArray(cmd, fullscreenVao);

			// Start Shader Program
			device->UseProgram(effect->shader->ShaderProgram());

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
			// See IEffect.h's comment on pipelineHandle/
			// pipelineBuiltForSwapchainGeneration: only the *last* effect's
			// pipeline (swapchain present pass) targets the swapchain
			// directly and can go stale when it's resized - real,
			// reproduced bug, not a hypothetical. Every other effect's
			// pipeline targets its own stable Texture-backed FBO and never
			// needs this rebuild (GetSwapchainGeneration() only tracks the
			// swapchain's own render pass), so the generation check is
			// gated on being the last effect.
			bool pipelineStale = isLastEffect && effect->pipelineHandle != 0 &&
				effect->pipelineBuiltForSwapchainGeneration != device->GetSwapchainGeneration();
			if (pipelineStale)
			{
				device->DestroyPipeline(effect->pipelineHandle);
				effect->pipelineHandle = 0;
			}
			if (effect->pipelineHandle == 0)
			{
				IRenderDevice::PipelineDesc pdesc;
				pdesc.shaderProgram = effect->shader->ShaderProgram();
				pdesc.depthTest = false;
				pdesc.depthWrite = false;
				pdesc.blendingEnabled = false;
				pdesc.cullFace = CullFace::DoubleSided;
				pdesc.noVertexInput = true;
				effect->pipelineHandle = device->CreatePipeline(pdesc);
				effect->pipelineBuiltForSwapchainGeneration = device->GetSwapchainGeneration();
			}
			device->BindPipeline(cmd, effect->pipelineHandle);

			// Bind MRT
			for (std::vector<RTT::Info>::iterator i = effect->RTTOrder.begin(); i != effect->RTTOrder.end(); i++)
			{
				switch ((*i).Type)
				{
				case RTT::Color:
					// sceneFrame, not Color - see SetSceneSourceTexture().
					sceneFrame->Bind();
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
			for (std::list<__UniformPostProcess>::iterator i = effect->Uniforms.begin(); i != effect->Uniforms.end(); i++)
			{
				if ((*i).handle == -2)
				{
					(*i).handle = Shader::GetUniformLocation(effect->shader->ShaderProgram(), (*i).uniform.Name);
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
				case PostEffects::ViewFromScene:
				case PostEffects::InverseViewFromScene:
					// Nobody has supplied a view yet (see SetViewMatrix), so
					// fall back to whatever the effect itself holds - the same
					// thing Other does. SSAOEffect::SetViewMatrix() writes
					// straight into the uniform, and the demos drive it from
					// Lua that way; taking an early `break` here would leave
					// valuePtr NULL and quietly stop packing their matrix into
					// the extras UBO, which is the only route these uniforms
					// have on Vulkan.
					(*i).uniform.Type = Uniforms::DataType::Matrix;
					if (!haveViewMatrix)
					{
						valuePtr = (*i).uniform.Value.empty() ? NULL : &(*i).uniform.Value[0];
						valueSize = (uint32)(*i).uniform.Value.size();
						break;
					}
					if ((*i).uniform.Usage == PostEffects::ViewFromScene)
					{
						valuePtr = &viewMatrix.m; valueSize = sizeof(viewMatrix.m);
					}
					else
					{
						valuePtr = &viewMatrixInverse.m; valueSize = sizeof(viewMatrixInverse.m);
					}
					break;
				default:
				case PostEffects::Other:
					valuePtr = (*i).uniform.Value.empty() ? NULL : &(*i).uniform.Value[0];
					valueSize = (uint32)(*i).uniform.Value.size();
					break;
				}

				if (!effect->extraUniformOffsets.empty() && valuePtr != NULL)
				{
					std::map<std::string, uint32>::const_iterator offIt = effect->extraUniformOffsets.find((*i).uniform.Name);
					if (offIt != effect->extraUniformOffsets.end() && offIt->second + valueSize <= effect->extraUniformsScratch.size())
						memcpy(&effect->extraUniformsScratch[offIt->second], valuePtr, valueSize);
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
					case PostEffects::ViewFromScene:
					case PostEffects::InverseViewFromScene:
						// Same fallback as above: the effect's own value when
						// no view has been supplied. Sending nothing at all is
						// not an option - an unwritten mat4 is garbage.
						if (!haveViewMatrix)
						{
							if (!(*i).uniform.Value.empty()) Shader::SendUniform((*i).uniform, (*i).handle);
						}
						else if ((*i).uniform.Usage == PostEffects::ViewFromScene)
							Shader::SendUniform((*i).uniform, &viewMatrix.m, (*i).handle);
						else
							Shader::SendUniform((*i).uniform, &viewMatrixInverse.m, (*i).handle);
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
			if (effect->extraUniformsBinding != 0)
			{
				if (effect->extraUniformsBufferHandle == 0)
					effect->extraUniformsBufferHandle = device->CreateUniformBuffer(effect->extraUniformsSize, effect->extraUniformsBinding);
				// This effect's own buffer, explicitly - see
				// IRenderDevice::BindUniformBlockIfPresent(): two live
				// PostEffectsManagers (the editor runs one per viewport plus
				// one per material preview) each allocate an effect buffer
				// at the same binding, and the device's global binding-point
				// registry only remembers whichever came last.
				device->BindUniformBlockIfPresent(effect->shader->ShaderProgram(), effect->extraUniformsBlockName, effect->extraUniformsBinding, effect->extraUniformsBufferHandle);
				device->ReplaceUniformBuffer(effect->extraUniformsBufferHandle, effect->extraUniformsSize, &effect->extraUniformsScratch[0]);
			}

			device->DrawArrays(device->TranslateDrawType(DrawingType::Triangles), 0, 3);
			device->EndCommandBuffer(cmd);

			// Unbind MRT
			for (std::vector<RTT::Info>::reverse_iterator i = effect->RTTOrder.rbegin(); i != effect->RTTOrder.rend(); i++)
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
		};

		// Offscreen ping-pong only - keep BeginFrame/EndFrame (fence wait,
		// acquire, submit, present) *outside* this scope so the profiler
		// does not attribute the whole swapchain frame to "PostFX" the
		// way Deferred.RenderScene used to own them before Capture.
		{
			PYROS_PROFILE_SCOPE("PostFX.Process");
			// One short of the end normally - the last effect is the
			// swapchain pass below. Staying offscreen means it is just
			// another effect and the loop runs the lot.
			const size_t offscreenCount = renderLastToTexture ? effects.size() : (effects.size() - 1);
			for (size_t idx = 0; idx < offscreenCount; ++idx)
			{
				IEffect *effect = effects[idx];
				activeFBO = effect->fbo;
				device->SetViewport(0, 0, effect->Width, effect->Height);
				activeFBO->Bind();
				drawEffect(effect, false);
				activeFBO->UnBind();
				LastRTT = activeFBO->GetAttachments()[0]->TexturePTR;
			}
		}

		// Last effect draws to the swapchain, not an offscreen FBO - a
		// no-op on GL (no acquire/present step), but on Vulkan nothing
		// else in this class's callers ever acquires+presents a swapchain
		// frame: every example using PostEffectsManager wraps its *only*
		// RenderScene() call in CaptureFrame()/EndCapture() (an offscreen
		// FBO), so RenderScene()'s own isMainSwapchainPass gate always
		// skips BeginFrame()/EndFrame() there. Without this call, the
		// swapchain is never acquired for the whole frame - found via
		// every PostEffectsManager-based example
		// (SSAOExample/DepthOfField/MotionBlurExample) hanging forever in
		// vkWaitForFences() on this loop's *next* offscreen FBO fence
		// (or, for a single-effect chain, this exact draw's own eventual
		// submit) - a real GPU completion signal that, per MoltenVK's own
		// behavior on this machine, never arrives without an actual
		// swapchain present somewhere in the frame to drive it.
		if (renderLastToTexture)
		{
			// Nothing left to present: the loop above ended in an FBO, and
			// LastRTT is its texture. Restoring the scene's clear colour is
			// still owed (see sceneClearColor's comment).
			finalTexture = LastRTT;
			device->UseProgram(0);
			device->SetClearColor(sceneClearColor);
			return;
		}

		IEffect *lastEffect = effects.back();
		device->BeginFrame();
		device->SetViewport(0, 0, Width, Height);
		{
			PYROS_PROFILE_SCOPE("PostFX.Present");
			drawEffect(lastEffect, true);
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

		// See sceneClearColor's comment above - hand the scene's own clear
		// colour back so the next frame's CaptureFrame() bind clears to it.
		device->SetClearColor(sceneClearColor);
	}

	PostEffectsManager::~PostEffectsManager()
	{
		// Same in-flight-submission hazard as RemoveAllEffects() below, and
		// the same fix - at shutdown the last frame's command buffer is
		// routinely still executing, so tearing these down unguarded is a
		// real use-after-free (it shows up as an intermittent segfault on
		// quit rather than a clean exit).
		device->WaitIdle();
		if (fullscreenVao != 0)
		{
			device->DeleteVertexArray(fullscreenVao);
			fullscreenVao = 0;
		}
		for (std::vector<IEffect*>::iterator i = effects.begin(); i != effects.end(); i++)
		{
			delete (*i);
		}

		delete ExternalFBO;

		// Destroy Textures
		delete Color;
		delete Depth;

		if (viewportGammaEffect != NULL)
		{
			delete viewportGammaEffect;
			viewportGammaEffect = NULL;
		}
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

	// finalTexture belongs to an effect that is about to be deleted.
	void PostEffectsManager::RemoveAllEffects()
	{
		// An IEffect owns real GPU resources (its FBO/textures, and on
		// Vulkan the render pass + every pipeline cached against it). The
		// previous frame's submission can still be in flight here - the
		// frame fence is only waited on at the top of the *next*
		// BeginFrame() - so deleting them straight away destroys objects a
		// live command buffer is still using: confirmed as a real
		// VUID-vkDestroyPipeline-pipeline-00765 under validation layers,
		// on nothing more exotic than switching demos in DemoLauncher.
		// Same reasoning (and same fix) as DeferredRenderer::Resize()'s
		// leading WaitIdle - see its comment.
		device->WaitIdle();
		for (std::vector<IEffect*>::iterator i = effects.begin(); i != effects.end(); i++)
		{
			delete (*i);
		}
		effects.clear();
		// Pointed into an effect that has just been deleted; GetFinalTexture()
		// falls back to the capture until a new chain produces one.
		finalTexture = NULL;
		LastRTT = NULL;
	}

}
