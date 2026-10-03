//============================================================================
// Name        : AntiAliasingStage.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : What PostEffectsManager runs for the anti-aliasing setting.
//============================================================================

#include <Pyros3D/Rendering/PostEffects/AntiAliasingStage.h>
#include <Pyros3D/Rendering/PostEffects/Effects/AntiAliasingEffects.h>
#include <Pyros3D/Rendering/PostEffects/Effects/ResizeEffect.h>
#include <Pyros3D/Materials/IMaterial.h>
#include <Pyros3D/Rendering/Components/Rendering/RenderingComponent.h>
#include <Pyros3D/Ext/smaa/AreaTex.h>
#include <Pyros3D/Ext/smaa/SearchTex.h>
#include <cstdio>

namespace p3d {

	namespace {

		f32 Halton(uint32 index, const uint32 base)
		{
			f32 f = 1.f, r = 0.f;
			while (index > 0)
			{
				f /= (f32)base;
				r += f * (f32)(index % base);
				index /= base;
			}
			return r;
		}

		// Eight samples: enough to cover a pixel evenly, few enough that a
		// still camera converges in well under a second.
		const uint32 kJitterSamples = 8;
	}

	AntiAliasingStage::AntiAliasingStage(const uint32 width, const uint32 height)
		: Width(width), Height(height),
		  requested(AntiAliasingMode::Off), effective(AntiAliasingMode::Off),
		  deferred(false), pending(false), preserveDepth(false),
		  msColor(NULL), msDepth(NULL), msFBO(NULL), depthResolve(NULL),
		  taaCurrent(0), historyValid(false), frameIndex(0), havePrevViewProjection(false),
		  fxaa(NULL), smaaEdge(NULL), smaaWeight(NULL), smaaBlend(NULL),
		  areaTex(NULL), searchTex(NULL), copyPass(NULL)
	{
		taa[0] = taa[1] = NULL;
	}

	AntiAliasingStage::~AntiAliasingStage()
	{
		DestroyResources();
		delete copyPass;
	}

	void AntiAliasingStage::Request(const AntiAliasingMode mode, const bool isDeferred)
	{
		const uint32 maxSamples = GetActiveRenderDevice().GetMaxSamples();
		const AntiAliasingMode resolved = AntiAliasing::Resolve(mode, isDeferred, maxSamples);
		if (mode == requested && isDeferred == deferred && resolved == effective)
			return;

		const std::string reason = AntiAliasing::FallbackReason(mode, isDeferred, maxSamples);
		if (!reason.empty())
			fprintf(stderr, "Anti-aliasing: %s\n", reason.c_str());

		requested = mode;
		deferred = isDeferred;
		effective = resolved;
		pending = true;
	}

	bool AntiAliasingStage::HasPasses() const
	{
		return taa[0] != NULL || fxaa != NULL || smaaBlend != NULL;
	}

	void AntiAliasingStage::DestroyResources()
	{
		// Callers wait for the GPU first - see PostEffectsManager's
		// RemoveAllEffects() for why that matters.
		delete depthResolve; depthResolve = NULL;
		delete msFBO; msFBO = NULL;
		delete msColor; msColor = NULL;
		delete msDepth; msDepth = NULL;
		delete taa[0]; delete taa[1]; taa[0] = taa[1] = NULL;
		delete fxaa; fxaa = NULL;
		delete smaaEdge; smaaEdge = NULL;
		delete smaaWeight; smaaWeight = NULL;
		delete smaaBlend; smaaBlend = NULL;
		delete areaTex; areaTex = NULL;
		delete searchTex; searchTex = NULL;
	}

	void AntiAliasingStage::Apply(Texture* velocity, Texture* velocityDepth, Texture* captureDepth)
	{
		if (!pending)
			return;
		pending = false;

		IRenderDevice &device = GetActiveRenderDevice();
		device.WaitIdle();
		DestroyResources();
		historyValid = false;
		havePrevViewProjection = false;
		frameIndex = 0;

		switch (effective)
		{
		case AntiAliasingMode::FXAA:
			fxaa = new FXAAEffect(Width, Height);
			break;

		case AntiAliasingMode::SMAA:
		{
			// The reference lookup tables, uploaded as-is: row 0 of the
			// array lands at v = 0 on every backend, which is the row SMAA
			// computes its coordinates against.
			areaTex = new Texture();
			areaTex->CreateEmptyTexture(TextureType::Texture, TextureDataType::RG8, AREATEX_WIDTH, AREATEX_HEIGHT, false);
			areaTex->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
			areaTex->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
			areaTex->UpdateData((void*)areaTexBytes);
			searchTex = new Texture();
			searchTex->CreateEmptyTexture(TextureType::Texture, TextureDataType::R8, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, false);
			searchTex->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
			searchTex->SetMinMagFilter(TextureFilter::Linear, TextureFilter::Linear);
			searchTex->UpdateData((void*)searchTexBytes);

			smaaEdge = new SMAAEdgeEffect(Width, Height);
			smaaWeight = new SMAAWeightEffect(smaaEdge->GetTexture(), areaTex, searchTex, Width, Height);
			smaaBlend = new SMAABlendEffect(smaaWeight->GetTexture(), Width, Height);
			break;
		}

		case AntiAliasingMode::TAA:
			if (velocity == NULL || velocityDepth == NULL)
			{
				fprintf(stderr, "Anti-aliasing: TAA needs the velocity pass and none was given - running without anti-aliasing\n");
				break;
			}
			taa[0] = new TAAResolveEffect(velocity, velocityDepth, Width, Height);
			taa[1] = new TAAResolveEffect(velocity, velocityDepth, Width, Height);
			taa[0]->SetHistory(taa[1]->GetTexture());
			taa[1]->SetHistory(taa[0]->GetTexture());
			taaCurrent = 0;
			break;

		case AntiAliasingMode::MSAA2x:
		case AntiAliasingMode::MSAA4x:
		case AntiAliasingMode::MSAA8x:
		{
			const uint32 samples = AntiAliasing::SampleCount(effective);
			// Same formats as the single-sample capture, so the resolve is a
			// straight copy. No filter or wrap calls: GL rejects sampler
			// state on a multisample texture.
			msColor = new Texture();
			msColor->CreateEmptyTexture(TextureType::Texture_Multisample, TextureDataType::RGBA16F, Width, Height, false, 0, samples);
			msDepth = new Texture();
			msDepth->CreateEmptyTexture(TextureType::Texture_Multisample, TextureDataType::DepthComponent, Width, Height, false, 0, samples);
			msFBO = new FrameBuffer();
			msFBO->SetDebugName("Post effects capture (MSAA)");
			msFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture_Multisample, msDepth);
			msFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture_Multisample, msColor);
			if (preserveDepth)
				device.SetFramebufferPreserveDepth(msFBO->GetBindID(), true);
			if (!device.CanBlitResolveDepth() && captureDepth != NULL)
				depthResolve = new MSDepthResolveEffect(msDepth, captureDepth, Width, Height);
			break;
		}

		case AntiAliasingMode::Off:
		default:
			break;
		}
	}

	void AntiAliasingStage::Resize(const uint32 width, const uint32 height)
	{
		if (width == 0 || height == 0 || (width == Width && height == Height))
			return;
		Width = width;
		Height = height;

		if (msFBO != NULL)
			msFBO->Resize(Width, Height);
		if (depthResolve != NULL)
		{
			// Its target is the capture's depth, which the manager resizes
			// with the capture - only the viewport follows here.
			depthResolve->Width = Width;
			depthResolve->Height = Height;
		}
		IEffect* passes[] = { taa[0], taa[1], fxaa, smaaEdge, smaaWeight, smaaBlend, copyPass };
		for (IEffect* e : passes)
			if (e != NULL) e->Resize(Width, Height);

		// A history the size of the old frame reprojects into the wrong
		// texels.
		historyValid = false;
	}

	void AntiAliasingStage::SetPreserveDepth(const bool preserve)
	{
		preserveDepth = preserve;
		if (msFBO != NULL)
			GetActiveRenderDevice().SetFramebufferPreserveDepth(msFBO->GetBindID(), preserve);
	}

	void AntiAliasingStage::ResolveMultisample(FrameBuffer* target, const bool withDepth, const DeviceHandle fullscreenVao)
	{
		if (msFBO == NULL || target == NULL)
			return;
		IRenderDevice &device = GetActiveRenderDevice();

		// Colour and depth in one bind of the target. On Metal binding for
		// Write opens a clearing render pass, so a second bind between the
		// two would wipe whichever resolve went first.
		msFBO->Bind(FBOAccess::Read);
		target->Bind(FBOAccess::Write);
		FrameBuffer::BlitFrameBuffer(0, 0, Width, Height, 0, 0, Width, Height, FBOBufferBit::Color, FBOFilter::Nearest);
		if (withDepth && device.CanBlitResolveDepth())
			FrameBuffer::BlitFrameBuffer(0, 0, Width, Height, 0, 0, Width, Height, FBOBufferBit::Depth, FBOFilter::Nearest);
		target->UnBind();
		msFBO->UnBind();

		if (withDepth && depthResolve != NULL)
			DrawDepthResolve(fullscreenVao);
	}

	void AntiAliasingStage::DrawDepthResolve(const DeviceHandle fullscreenVao)
	{
		IRenderDevice &device = GetActiveRenderDevice();
		IEffect* effect = depthResolve;

		effect->fbo->Bind();
		device.SetViewport(0, 0, Width, Height);
		CommandBufferHandle cmd = device.BeginCommandBuffer();
		device.BindVertexArray(cmd, fullscreenVao);
		device.UseProgram(effect->shader->ShaderProgram());
		if (effect->pipelineHandle == 0)
		{
			IRenderDevice::PipelineDesc pdesc;
			pdesc.shaderProgram = effect->shader->ShaderProgram();
			// Every texel is written, whatever was there.
			pdesc.depthTest = true;
			pdesc.depthTestMode = DepthTest::Always;
			pdesc.depthWrite = true;
			pdesc.blendingEnabled = false;
			pdesc.cullFace = CullFace::DoubleSided;
			pdesc.noVertexInput = true;
			effect->pipelineHandle = device.CreatePipeline(pdesc);
		}
		device.BindPipeline(cmd, effect->pipelineHandle);

		Texture::ResetUnitCounter();
		msDepth->Bind();
		for (std::list<__UniformPostProcess>::iterator i = effect->Uniforms.begin(); i != effect->Uniforms.end(); i++)
		{
			if ((*i).handle == -2)
				(*i).handle = Shader::GetUniformLocation(effect->shader->ShaderProgram(), (*i).uniform.Name);
			if ((*i).handle != -1)
				Shader::SendUniform((*i).uniform, (*i).handle);
		}
		device.DrawArrays(device.TranslateDrawType(DrawingType::Triangles), 0, 3);
		device.EndCommandBuffer(cmd);
		msDepth->Unbind();
		effect->fbo->UnBind();
		device.UseProgram(0);
	}

	IEffect* AntiAliasingStage::PrepareTAA(const Matrix &viewProjection, const bool haveViewProjection)
	{
		if (taa[0] == NULL)
			return NULL;
		TAAResolveEffect* current = taa[taaCurrent];

		// Previous clip space from this one, for texels with no velocity
		// written. Identity (no camera motion) until two frames are known.
		Matrix reproject;
		if (haveViewProjection && havePrevViewProjection)
			reproject = prevViewProjection * viewProjection.Inverse();
		current->SetFrameParams(reproject, historyValid);

		if (haveViewProjection)
		{
			prevViewProjection = viewProjection;
			havePrevViewProjection = true;
		}
		return current;
	}

	void AntiAliasingStage::AppendFinalPasses(std::vector<IEffect*> &out)
	{
		if (fxaa != NULL)
			out.push_back(fxaa);
		if (smaaBlend != NULL)
		{
			out.push_back(smaaEdge);
			out.push_back(smaaWeight);
			out.push_back(smaaBlend);
		}
	}

	void AntiAliasingStage::SetFinalInput(Texture* input)
	{
		if (fxaa != NULL) fxaa->SetColorOverride(input);
		if (smaaEdge != NULL) smaaEdge->SetColorOverride(input);
		if (smaaBlend != NULL) smaaBlend->SetColorOverride(input);
	}

	IEffect* AntiAliasingStage::GetCopyPass()
	{
		if (copyPass == NULL)
			copyPass = new ResizeEffect(RTT::LastRTT, Width, Height);
		return copyPass;
	}

	Vec2 AntiAliasingStage::GetJitter() const
	{
		if (taa[0] == NULL || Width == 0 || Height == 0)
			return Vec2(0.f, 0.f);
		// Halton(2,3) from index 1 - index 0 is (0,0) in both bases, a
		// corner rather than a sample. One pixel is 2/size in NDC.
		const uint32 i = (frameIndex % kJitterSamples) + 1;
		return Vec2((Halton(i, 2) - 0.5f) * 2.f / (f32)Width,
		            (Halton(i, 3) - 0.5f) * 2.f / (f32)Height);
	}

	void AntiAliasingStage::EndFrame(const bool ranTAA)
	{
		frameIndex++;
		if (ranTAA)
		{
			taaCurrent ^= 1;
			historyValid = true;
		}
	}

}
