//============================================================================
// Name        : DeferredRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Deferred Renderer
//============================================================================

#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Rendering/Terrain/TerrainHorizon.h>
#include <Pyros3D/Rendering/Terrain/TerrainOcclusion.h>
#include <Pyros3D/Other/PyrosGL.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>

namespace p3d {

	// Whether a sun's pass lays the ambient light as well (see RenderScene).
	static bool g_sunLaysAmbient = false;
	// Whether the G-buffer is drawn nearest object first (see RenderScene).
	static bool g_gbufferNearestFirst = true;
	void DeferredRenderer::SetNearestFirst(const bool on) { g_gbufferNearestFirst = on; }
	bool DeferredRenderer::GetNearestFirst() { return g_gbufferNearestFirst; }
	void DeferredRenderer::SetSunLaysAmbient(const bool on) { g_sunLaysAmbient = on; }
	bool DeferredRenderer::GetSunLaysAmbient() { return g_sunLaysAmbient; }


	// Ambient occlusion at half the frame's resolution: every deferred
	// renderer's, like the ambient light and the clear colour.
	static bool g_ssaoHalfResolution = false;
	void DeferredRenderer::SetSSAOHalfResolution(const bool half) { g_ssaoHalfResolution = half; }
	static bool g_ssaoTemporal = false;
	void DeferredRenderer::SetSSAOTemporal(const bool on) { g_ssaoTemporal = on; }
	bool DeferredRenderer::GetSSAOTemporal() { return g_ssaoTemporal; }
	bool DeferredRenderer::IsSSAOHalfResolution() { return g_ssaoHalfResolution; }


	f32 f(f32 r)
	{
		return r * (2.f * (tanf((f32)PI / 4.f)));
	}
	f32 g(f32 a)
	{
		return a / (2.f*sinf((f32)PI / 4.f));
	}

	DeferredRenderer::DeferredRenderer(const uint32 Width, const uint32 Height, FrameBuffer* fbo) : IRenderer(Width, Height)
	{

		echo("TRACE: Deferred Renderer Created");

		// (the sun's shadow maps may be recorded on another thread while this
		// records the scene: RenderScene waits for that before it returns)
		recordsBeside = true;

		ActivateCulling(CullingMode::FrustumCulling);

		shadowMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows);
		shadowMaterial->SetCullFace(CullFace::DoubleSided);
		shadowSkinnedMaterial = new GenericShaderMaterial(ShaderUsage::CastShadows | ShaderUsage::Skinning);
		shadowSkinnedMaterial->SetCullFace(CullFace::DoubleSided);

		// Real, found-in-this-investigation inconsistency: every sibling
		// render-target texture in this file (forwardDepthTexture right
		// below, and every G-buffer attachment the caller creates)
		// explicitly passes Mipmapping=false - this one didn't, silently
		// picking up CreateEmptyTexture()'s Mipmapping=true default. A
		// mipmapped render target still needs GetOrCreateRenderTargetView()'s
		// level-0-only view to even be usable as a framebuffer attachment
		// at all (see its comment) - correct on paper, but an unnecessary
		// difference from every other render target in this class for no
		// reason, on the one attachment (the whole second pass's output)
		// this session's black-screen investigation narrowed the problem
		// down to.
		// RGBA16F, not RGBA8 - see PostEffectsManager.cpp's identical
		// comment on its own Color texture. This is lastPassFBO's
		// additive light-accumulation target (ambient + N lights via
		// BlendFunc::One,One below); without float headroom here the
		// accumulation itself clips before a wrapping PostEffectsManager
		// (if any) ever gets a chance to capture it, regardless of that
		// buffer's own format.
		colorTexture = new Texture(); colorTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height, false);
		colorTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		// See DeferredRenderer.h's comment on previousFrameColorTexture -
		// material-aware SSR's reflection source. Its own tiny FBO exists
		// purely to give BlitFramebuffer() a bindable destination (same
		// pattern as MSAATest's resolvedFBO) - nothing ever draws into
		// this FBO the normal way, it's a pure blit target. Mipmapping
		// enabled (was false) for real roughness-based reflection blur -
		// see lastPass.glsl's uMaxReflectionLod/textureLod() comment;
		// mips are regenerated every frame after the blit that refreshes
		// this texture's content, in RenderScene() below.
		previousFrameColorTexture = new Texture(); previousFrameColorTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, Width, Height, true);
		previousFrameColorTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		previousFrameColorTexture->SetMinMagFilter(TextureFilter::LinearMipmapLinear, TextureFilter::Linear);
		previousFrameFBO = new FrameBuffer();
		previousFrameFBO->SetDebugName("Deferred previous frame");
		previousFrameFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, previousFrameColorTexture);

		// See DeferredRenderer.h's comment on forwardDepthTexture - a real
		// copy of fbo's depth attachment, refreshed every frame in
		// RenderScene(), used as lastPassFBO's depth attachment instead
		// of directly aliasing fbo's own depth texture.
		forwardDepthTexture = new Texture();
		forwardDepthTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
		forwardDepthTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		// See PostEffectsManager::Init() - a Linear-filtered depth texture is
		// "unloadable" on Apple GL and reads back as the zero texture. This
		// one is handed out by GetDepthTexture() and sampled.
		forwardDepthTexture->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);

		lastPassFBO = new FrameBuffer();
		lastPassFBO->SetDebugName("Deferred last pass");
		lastPassFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, colorTexture);
		lastPassFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, forwardDepthTexture);
		// This pass's depth is not scratch: RenderScene() copies the finished
		// G-buffer depth into forwardDepthTexture just before binding here,
		// so the translucent forward pass can depth-test against the opaque
		// scene. Only the color attachment is cleared on bind (see
		// ClearBufferBit(Color) there), which is implicit on GL but has to be
		// declared up front on Vulkan, where the load op is baked into the
		// render pass. Without this the copied depth was cleared away before
		// the first draw and every transparent object drew over all the
		// opaque geometry.
		device->SetFramebufferPreserveDepth(lastPassFBO->GetBindID(), true);

		// See DeferredRenderer.h's comment on dummyShadow2D/dummyShadowCube.
		// Same creation pattern DirectionalLight/PointLight's own real
		// EnableCastShadows() uses, just tiny (contents never sampled).
		dummyShadow2D = new Texture();
		dummyShadow2D->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, 4, 4, false);
		dummyShadow2D->SetRepeat(TextureRepeat::Clamp, TextureRepeat::Clamp);
		// Nearest as well - "same creation pattern" has to include this, or
		// the placeholder is exactly the depth+compare+Linear combination
		// Apple GL refuses to load (see DirectionalLight::EnableCastShadows).
		// The contents are never read, but the sampler is still bound, and
		// the driver logs the rejection every run.
		dummyShadow2D->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
		dummyShadow2D->EnableCompareMode();

		dummyShadowCube = new Texture();
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapNegative_X, TextureDataType::R32F, 4, 4, false);
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapNegative_Y, TextureDataType::R32F, 4, 4, false);
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapNegative_Z, TextureDataType::R32F, 4, 4, false);
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapPositive_X, TextureDataType::R32F, 4, 4, false);
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapPositive_Y, TextureDataType::R32F, 4, 4, false);
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapPositive_Z, TextureDataType::R32F, 4, 4, false);
		dummyShadowCube->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		// No compare mode, matching the real point-light shadow cubemap it
		// stands in for - secondpassPoint.glsl reads both as a plain
		// samplerCube (see its PCFPOINT comment).
		dummyShadowsWarmedUp = false;


		// Default View Port Init Values
		viewPortStartX = viewPortStartY = 0;

		// Save FrameBuffer
		FBO = fbo;

		// Create Second Pass Specifics
		deferredLastPass= new CustomShaderMaterial("shaders/lastPass.glsl");
		deferredMaterialAmbient= new CustomShaderMaterial("shaders/secondpassAmbient.glsl");
		deferredMaterialDirectional = new CustomShaderMaterial("shaders/secondpassDirectional.glsl");
		deferredMaterialPoint = new CustomShaderMaterial("shaders/secondpassPoint.glsl");
		deferredMaterialSpot = new CustomShaderMaterial("shaders/secondpassSpot.glsl");
		deferredSSAO = new CustomShaderMaterial("shaders/deferredSSAO.glsl");
		deferredSSAOBlur = new CustomShaderMaterial("shaders/deferredSSAOBlur.glsl");

		// tDepth/tNormal/tMetallicRoughness (units 0-2) re-bind the same
		// G-buffer attachments the lighting passes already sample, just
		// scoped to this one draw - see the fresh Bind() sequence around
		// this material's RenderObject() call in RenderScene(). tColor
		// moved from unit 0 to 3 (it used to be the only texture this
		// material sampled) to make room for them; tPreviousFrameColor
		// (unit 4) is material-aware SSR's reflection source - see
		// DeferredRenderer.h's comment on previousFrameColorTexture.
		uint32 ssrTexID = 0;
		deferredLastPass->AddUniform(Uniform("tDepth", Uniforms::DataType::Int, &ssrTexID));
		ssrTexID = 1;
		deferredLastPass->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &ssrTexID));
		ssrTexID = 2;
		deferredLastPass->AddUniform(Uniform("tMetallicRoughness", Uniforms::DataType::Int, &ssrTexID));
		uint32 colorID = 3;
		deferredLastPass->AddUniform(Uniform("tColor", Uniforms::DataType::Int, &colorID));
		ssrTexID = 4;
		deferredLastPass->AddUniform(Uniform("tPreviousFrameColor", Uniforms::DataType::Int, &ssrTexID));
		// Real albedo (not just roughness/metallic) - needed for
		// FresnelSchlick's F0 = mix(0.04, albedo, metallic), matching
		// CalculatePBRLighting's own formula exactly, so metals reflect
		// their own tint instead of a colorless approximation.
		ssrTexID = 5;
		deferredLastPass->AddUniform(Uniform("tDiffuse", Uniforms::DataType::Int, &ssrTexID));

		deferredLastPass->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		deferredLastPass->AddUniform(Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));
		deferredLastPass->AddUniform(Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));
		deferredLastPass->AddUniform(Uniform("uViewMatrixInverse", Uniforms::DataUsage::ViewMatrixInverse));
		// Previous frame's camera, for SSR's reflection reprojection.
		// Deliberately NOT IRenderer's shared PrvProjectionMatrix/
		// PrvViewMatrix via Uniforms::DataUsage - those get clobbered
		// inside PreRender() (ViewMatrix unconditionally, ProjectionMatrix
		// whenever any light in the scene casts shadows) before
		// RenderScene() ever runs, so shifting them here always captured
		// this frame's own already-current camera, not a real previous
		// one - reprojection was silently a no-op. See DeferredRenderer.h's
		// comment on ssrPrvViewMatrix/ssrPrvProjectionMatrix for the full
		// story; these two handles are fed manually from that dedicated,
		// PreRender()-immune state instead.
		lastPassPrvProjectionMatrixHandle = deferredLastPass->AddUniform(Uniform("uPrvProjectionMatrix", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		lastPassPrvProjectionMatrixHandle->SetValue(&ssrPrvProjectionMatrix);
		lastPassPrvViewMatrixHandle = deferredLastPass->AddUniform(Uniform("uPrvViewMatrix", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		lastPassPrvViewMatrixHandle->SetValue(&ssrPrvViewMatrix);
		// Real mip count of previousFrameColorTexture - see lastPass.glsl's
		// uMaxReflectionLod/textureLod() comment. Not a DataUsage the
		// renderer computes generically (it's this material's own
		// texture, not a camera/frame property), so a plain handle +
		// SetValue() each frame, same pattern as pointRadiusHandle etc.
		lastPassMaxReflectionLodHandle = deferredLastPass->AddUniform(Uniform("uMaxReflectionLod", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		// Real, per-scene-settable SSR march distances - see
		// lastPass.glsl's identical comment on why these are uniforms,
		// not shader constants (an earlier, automatic per-pixel-depth-
		// scaled version was a real, shipped regression: reflections
		// landed near the horizon instead of under the object casting
		// them). Defaults match this shader's original, proven-correct
		// values; SetSSRDistances() below lets a caller retune them for
		// a scene built at a very different scale. Note 0.22 is below the
		// shader's stride floor of 1 pixel, so the default marches at
		// stride 1 - which is what it did back when the shader ignored
		// this uniform entirely. See SetSSRDistances()'s comment.
		ssrStepDistance = 0.22f;
		ssrMaxDistance = 40.0f;
		lastPassSSRStepDistanceHandle = deferredLastPass->AddUniform(Uniform("uSSRStepDistance", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		lastPassSSRStepDistanceHandle->SetValue(&ssrStepDistance);
		lastPassSSRMaxDistanceHandle = deferredLastPass->AddUniform(Uniform("uSSRMaxDistance", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		lastPassSSRMaxDistanceHandle->SetValue(&ssrMaxDistance);
		// Real opt-in gate - defaults OFF so demos that never asked for SSR
		// don't pay for the previous-frame blit/mip path. EnableSSR() opts
		// a specific instance in (SSRTest / DemoLauncher SSR Test).
		ssrEnabled = 0.0f;
		lastPassSSREnabledHandle = deferredLastPass->AddUniform(Uniform("uSSREnabled", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		lastPassSSREnabledHandle->SetValue(&ssrEnabled);
		ssrDebugMode = 0.0f;
		lastPassSSRDebugHandle = deferredLastPass->AddUniform(Uniform("uSSRDebug", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		lastPassSSRDebugHandle->SetValue(&ssrDebugMode);

		// See IMaterial.h's comment on extraUniforms[2] - matches the
		// LastPassFragParams block declared in shaders/lastPass.glsl
		// exactly. std140 offsets computed by hand, same discipline as
		// DirectionalFragParams below (vec2s pack at 8-byte strides, each
		// mat4 rounds up to the next 16-byte boundary).
		deferredLastPass->extraUniforms[0].binding = 37;
		deferredLastPass->extraUniforms[0].blockName = "LastPassFragParams";
		deferredLastPass->extraUniforms[0].size = 304;
		deferredLastPass->extraUniforms[0].scratch.resize(deferredLastPass->extraUniforms[0].size, 0);
		deferredLastPass->extraUniforms[0].offsets["uScreenDimensions"] = 0;
		deferredLastPass->extraUniforms[0].offsets["uNearFar"] = 8;
		deferredLastPass->extraUniforms[0].offsets["uMatProj"] = 16;
		deferredLastPass->extraUniforms[0].offsets["uViewMatrixInverse"] = 80;
		deferredLastPass->extraUniforms[0].offsets["uPrvProjectionMatrix"] = 144;
		deferredLastPass->extraUniforms[0].offsets["uPrvViewMatrix"] = 208;
		deferredLastPass->extraUniforms[0].offsets["uMaxReflectionLod"] = 272;
		deferredLastPass->extraUniforms[0].offsets["uSSRStepDistance"] = 276;
		deferredLastPass->extraUniforms[0].offsets["uSSRMaxDistance"] = 280;
		deferredLastPass->extraUniforms[0].offsets["uSSREnabled"] = 284;
		deferredLastPass->extraUniforms[0].offsets["uSSRDebug"] = 288;
		// PopulateAutoExtraUniforms() stashes the fragment UBO on slot [1]
		// (vertex has none). Hand-written offsets above go on [0]. Leaving
		// both at binding 37 made SendExtraUniforms upload two buffers to
		// the same slot - the auto one won and could disagree with these
		// offsets (uSSREnabled reading as 0 → zero SSR on Vulkan). Only
		// the hand layout is authoritative for this material.
		deferredLastPass->extraUniforms[1].binding = 0;
		deferredLastPass->extraUniforms[1].bufferHandle = 0;
		deferredLastPass->extraUniforms[1].size = 0;
		deferredLastPass->extraUniforms[1].offsets.clear();
		deferredLastPass->extraUniforms[1].scratch.clear();
		// Real, pre-existing inconsistency found while investigating a
		// separate rendering issue: every other second-pass material
		// (Ambient/Directional/Point/Spot, further below) explicitly
		// disables depth test/write for its full-screen-quad draw -
		// deferredLastPass was the only one left at IMaterial's default
		// (depthTest=true), which is never correct for a final blit
		// sampling an already-composited color buffer. Did not by itself
		// resolve the issue being investigated, but is a real fix on its
		// own merits (consistent with its four siblings) - kept.
		deferredLastPass->DisableDepthTest();
		deferredLastPass->DisableDepthWrite();
		// See deferredMaterialAmbient's identical comment further below -
		// same screen-space full-screen-quad pass, same backface-culling bug.
		deferredLastPass->SetCullFace(CullFace::DoubleSided);

		uint32 texID = 0;
		deferredMaterialAmbient->AddUniform(Uniform("tDepth", Uniforms::DataType::Int, &texID));
		deferredMaterialDirectional->AddUniform(Uniform("tDepth", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniform("tDepth", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tDepth", Uniforms::DataType::Int, &texID));
		texID = 1;
		deferredMaterialAmbient->AddUniform(Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
		deferredMaterialDirectional->AddUniform(Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tDiffuse", Uniforms::DataType::Int, &texID));
		texID = 2;
		deferredMaterialAmbient->AddUniform(Uniform("tSpecular", Uniforms::DataType::Int, &texID));
		deferredMaterialDirectional->AddUniform(Uniform("tSpecular", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniform("tSpecular", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tSpecular", Uniforms::DataType::Int, &texID));
		texID = 3;
		deferredMaterialAmbient->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));
		deferredMaterialDirectional->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &texID));
		// PBR metallic/roughness G-buffer attachment (Color_Attachment3) -
		// bound as texture unit 4, matching its AddAttach() order in the
		// caller's FBO setup (see FBO->GetAttachments() bind loop below).
		texID = 4;
		deferredMaterialAmbient->AddUniform(Uniform("tMetallicRoughness", Uniforms::DataType::Int, &texID));
		deferredMaterialDirectional->AddUniform(Uniform("tMetallicRoughness", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniform("tMetallicRoughness", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tMetallicRoughness", Uniforms::DataType::Int, &texID));

		// Bound after the five G-buffer attachments - see the ambient draw.
		texID = 5;
		deferredMaterialAmbient->AddUniform(Uniform("tAO", Uniforms::DataType::Int, &texID));
		deferredMaterialDirectional->AddUniform(Uniform("tAO", Uniforms::DataType::Int, &texID));
		deferredMaterialPoint->AddUniform(Uniform("tAO", Uniforms::DataType::Int, &texID));
		deferredMaterialSpot->AddUniform(Uniform("tAO", Uniforms::DataType::Int, &texID));
		deferredMaterialAmbient->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		deferredMaterialAmbient->AddUniform(Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));

		// See IMaterial.h's comment on extraUniforms[2] - matches the
		// AmbientFragParams block declared in shaders/secondpassAmbient.glsl
		// exactly. uMatProj registered above is never actually declared in
		// that shader (a pre-existing, harmless dead registration - GL's
		// glGetUniformLocation() already silently no-ops it), so it has no
		// entry here either.
		deferredMaterialAmbient->extraUniforms[0].binding = 27;
		deferredMaterialAmbient->extraUniforms[0].blockName = "AmbientFragParams";
		deferredMaterialAmbient->extraUniforms[0].size = 8;
		deferredMaterialAmbient->extraUniforms[0].scratch.resize(deferredMaterialAmbient->extraUniforms[0].size, 0);
		deferredMaterialAmbient->extraUniforms[0].offsets["uScreenDimensions"] = 0;

		deferredMaterialAmbient->DisableDepthTest();
		deferredMaterialAmbient->DisableDepthWrite();
		deferredMaterialAmbient->EnableBlending();
		deferredMaterialAmbient->BlendingEquation(BlendEq::Add);
		// One,Zero (not One,One like the point/spot/directional lights below)
		// - ambient is always the *first* thing drawn into lastPassFBO after
		// ClearScreen(), and that clear uses DrawBackground()'s clear colour
		// (device-global state, still whatever the scene's background is,
		// not black - see DrawBackground()'s comment), not (0,0,0,x). With
		// One,One this pass added its own lit result *on top of* that
		// leftover background colour for every geometry pixel (background/
		// sky pixels are unaffected - secondpassAmbient.glsl discards those),
		// uniformly over-brightening every object by the scene's background
		// colour - e.g. a 0.2/0.2/0.2 grey editor viewport background made
		// Deferred's ambient-only spheres visibly brighter than Forward's,
		// for the exact same ambient light. One,Zero replaces the stale
		// clear value outright for the pixels this pass actually writes,
		// while leaving discarded background pixels (which never reach the
		// blend stage at all) exactly as ClearScreen() left them.
		deferredMaterialAmbient->BlendingFunction(BlendFunc::One, BlendFunc::Zero);
		// THE root cause of the Vulkan black-screen investigation: this is a
		// screen-space full-screen-quad pass (vertex shader is a trivial
		// gl_Position = vec4(aPosition,1.0) passthrough, no projection
		// matrix - see secondpassAmbient.glsl) but IMaterial defaults every
		// material to CullFace::BackFace. On GL that convention happens to
		// let the quad through; on Vulkan it does not, so 100% of its
		// fragments were being backface-culled before the fragment shader
		// ever ran - explains why even a hardcoded solid-color FragColor
		// never showed up in lastPassFBO's colorTexture. PostEffectsManager
		// hits the exact same class of pass and already force-overrides to
		// CullFace::DoubleSided for this reason (see its CreatePipeline
		// call) - a full-screen quad has no meaningful back face to cull.
		deferredMaterialAmbient->SetCullFace(CullFace::DoubleSided);

		// SSAO - see EnableSSAO(). Defaults suit a scene built in metres.
		ssaoEnabled = false;
		ssaoRadius = 0.5f;
		ssaoStrength = 2.0f;
		ssaoFalloff = 0.5f;
		ssaoSamples = 16.0f;
		ssaoDirect = 1.0f;
		uint32 ssaoUnit = 0;
		deferredSSAO->AddUniform(Uniform("tDepth", Uniforms::DataType::Int, &ssaoUnit));
		deferredSSAOBlur->AddUniform(Uniform("tAO", Uniforms::DataType::Int, &ssaoUnit));
		ssaoUnit = 1;
		deferredSSAO->AddUniform(Uniform("tNormal", Uniforms::DataType::Int, &ssaoUnit));
		deferredSSAOBlur->AddUniform(Uniform("tDepth", Uniforms::DataType::Int, &ssaoUnit));
		SetupSSAOMaterial(deferredSSAO, ssaoHandles[0]);
		SetupSSAOMaterial(deferredSSAOBlur, ssaoHandles[1]);
		ssaoTexture = new Texture();
		ssaoTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
		ssaoTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		ssaoFBO = new FrameBuffer();
		ssaoFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, ssaoTexture);
		ssaoBlurTexture = new Texture();
		ssaoBlurTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, Width, Height, false);
		ssaoBlurTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		ssaoBlurFBO = new FrameBuffer();
		ssaoBlurFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, ssaoBlurTexture);
		ssaoWhite = new Texture();
		ssaoWhite->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, 1, 1, false);
		{
			uchar pixel[4] = { 255, 255, 255, 255 };
			ssaoWhite->UpdateData(pixel);
		}

		deferredMaterialDirectional->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		dirDirHandle = deferredMaterialDirectional->AddUniform(Uniform("uLightDirection", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
		dirColorHandle = deferredMaterialDirectional->AddUniform(Uniform("uLightColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		dirShadowHandle = deferredMaterialDirectional->AddUniform(Uniform("uShadowMap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		dirShadowPCFTexelHandle = deferredMaterialDirectional->AddUniform(Uniform("uPCFTexelSize", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		dirShadowDepthsMVPHandle = deferredMaterialDirectional->AddUniform(Uniform("uDirectionalDepthsMVP", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		dirShadowFarHandle = deferredMaterialDirectional->AddUniform(Uniform("uDirectionalShadowFar", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		dirHaveShadowHandle = deferredMaterialDirectional->AddUniform(Uniform("uHaveShadowmap", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		dirAmbientTooHandle = deferredMaterialDirectional->AddUniform(Uniform("uAmbientToo", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		// The terrain's baked shadow (TerrainHorizon), where the scene has one.
		dirHorizonMapHandle = deferredMaterialDirectional->AddUniform(Uniform("uHorizonMap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		dirHorizonRectHandle = deferredMaterialDirectional->AddUniform(Uniform("uHorizonRect", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		dirHorizonSunHandle = deferredMaterialDirectional->AddUniform(Uniform("uHorizonSun", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		deferredMaterialDirectional->AddUniform(Uniform("uViewInverse", Uniforms::DataUsage::ViewMatrixInverse));
		deferredMaterialDirectional->AddUniform(Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialDirectional->AddUniform(Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));

		// See IMaterial.h's comment on extraUniforms[2] - matches the
		// DirectionalFragParams block declared in
		// shaders/secondpassDirectional.glsl exactly, std140 offsets
		// computed by hand (vec2/vec3/vec4/vec2 pack per their own
		// alignment, each mat4 rounds up to the next 16-byte boundary).
		deferredMaterialDirectional->extraUniforms[0].binding = 32;
		deferredMaterialDirectional->extraUniforms[0].blockName = "DirectionalFragParams";
		deferredMaterialDirectional->extraUniforms[0].size = 528;
		deferredMaterialDirectional->extraUniforms[0].scratch.resize(deferredMaterialDirectional->extraUniforms[0].size, 0);
		deferredMaterialDirectional->extraUniforms[0].offsets["uScreenDimensions"] = 0;
		deferredMaterialDirectional->extraUniforms[0].offsets["uLightDirection"] = 16;
		deferredMaterialDirectional->extraUniforms[0].offsets["uLightColor"] = 32;
		deferredMaterialDirectional->extraUniforms[0].offsets["uNearFar"] = 48;
		deferredMaterialDirectional->extraUniforms[0].offsets["uMatProj"] = 64;
		deferredMaterialDirectional->extraUniforms[0].offsets["uPCFTexelSize"] = 128;
		deferredMaterialDirectional->extraUniforms[0].offsets["uDirectionalDepthsMVP"] = 144;
		deferredMaterialDirectional->extraUniforms[0].offsets["uDirectionalShadowFar"] = 400;
		deferredMaterialDirectional->extraUniforms[0].offsets["uHaveShadowmap"] = 416;
		deferredMaterialDirectional->extraUniforms[0].offsets["uAmbientToo"] = 420;
		// (the mat4 rounds up from 420 to the next 16: 432)
		deferredMaterialDirectional->extraUniforms[0].offsets["uViewInverse"] = 432;
		deferredMaterialDirectional->extraUniforms[0].offsets["uHorizonRect"] = 496;
		deferredMaterialDirectional->extraUniforms[0].offsets["uHorizonSun"] = 512;

		deferredMaterialDirectional->DisableDepthTest();
		deferredMaterialDirectional->DisableDepthWrite();
		deferredMaterialDirectional->EnableBlending();
		deferredMaterialDirectional->BlendingEquation(BlendEq::Add);
		deferredMaterialDirectional->BlendingFunction(BlendFunc::One, BlendFunc::One);
		// See deferredMaterialAmbient's identical comment above - same
		// screen-space full-screen-quad pass, same backface-culling bug.
		deferredMaterialDirectional->SetCullFace(CullFace::DoubleSided);

		deferredMaterialPoint->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		pointPosHandle = deferredMaterialPoint->AddUniform(Uniform("uLightPosition", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
		pointRadiusHandle = deferredMaterialPoint->AddUniform(Uniform("uLightRadius", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		pointColorHandle = deferredMaterialPoint->AddUniform(Uniform("uLightColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		pointShadowHandle = deferredMaterialPoint->AddUniform(Uniform("uShadowMap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		pointShadowPCFTexelHandle = deferredMaterialPoint->AddUniform(Uniform("uPCFTexelSize", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		pointShadowDepthsMVPHandle = deferredMaterialPoint->AddUniform(Uniform("uPointDepthsMVP", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		pointHaveShadowHandle = deferredMaterialPoint->AddUniform(Uniform("uHaveShadowmap", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		pointShadowBiasHandle = deferredMaterialPoint->AddUniform(Uniform("uShadowBias", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		pointVolumetricHandle = deferredMaterialPoint->AddUniform(Uniform("uVolumetricParams", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		deferredMaterialPoint->AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));
		// Real, pre-existing bug (not something this session's SSR work
		// introduced - the sphere light-volume technique with no depth
		// test has always had this) found chasing a report that mouse-
		// look pitch made point-light illumination disappear. Standard
		// deferred-shading gap: with no depth test, the ONLY thing that
		// puts a light's contribution on screen is its sphere proxy
		// actually getting rasterized - and when the camera is near or
		// inside the light's radius, the sphere's own vertices can end
		// up entirely behind the near clip plane, so the GPU clips the
		// whole primitive away before rasterization ever runs, before
		// CullFace gets a say. RenderScene() below detects this per-light
		// (distance to camera vs radius) and swaps in a real full-screen
		// quad (directionalLight's mesh, already used by the ambient/
		// directional passes for exactly this "guaranteed full coverage"
		// purpose) instead of the sphere - this flag tells the vertex
		// shader to skip the sphere's MVP transform and emit that quad's
		// own already-correct clip-space positions directly, matching
		// secondpassAmbient.glsl/secondpassDirectional.glsl's identical
		// `gl_Position = vec4(aPosition,1.0)` pattern.
		pointUseFullscreenQuadHandle = deferredMaterialPoint->AddUniform(Uniform("uUseFullscreenQuad", Uniforms::DataUsage::Other, Uniforms::DataType::Float));

		// See IMaterial.h's comment on extraUniforms[2] - two separate
		// blocks matching secondpassPoint.glsl's PointVertParams (VERTEX
		// stage, binding 33) and PointFragParams (FRAGMENT stage, binding
		// 38) exactly - see that file's comment on why it's two blocks,
		// not one combined block used by both stages.
		deferredMaterialPoint->extraUniforms[0].binding = 33;
		deferredMaterialPoint->extraUniforms[0].blockName = "PointVertParams";
		deferredMaterialPoint->extraUniforms[0].size = 196;
		deferredMaterialPoint->extraUniforms[0].scratch.resize(deferredMaterialPoint->extraUniforms[0].size, 0);
		deferredMaterialPoint->extraUniforms[0].offsets["uProjectionMatrix"] = 0;
		deferredMaterialPoint->extraUniforms[0].offsets["uViewMatrix"] = 64;
		deferredMaterialPoint->extraUniforms[0].offsets["uModelMatrix"] = 128;
		deferredMaterialPoint->extraUniforms[0].offsets["uUseFullscreenQuad"] = 192;

		deferredMaterialPoint->extraUniforms[1].binding = 38;
		deferredMaterialPoint->extraUniforms[1].blockName = "PointFragParams";
		deferredMaterialPoint->extraUniforms[1].size = 224;
		deferredMaterialPoint->extraUniforms[1].scratch.resize(deferredMaterialPoint->extraUniforms[1].size, 0);
		deferredMaterialPoint->extraUniforms[1].offsets["uScreenDimensions"] = 0;
		deferredMaterialPoint->extraUniforms[1].offsets["uLightPosition"] = 16;
		deferredMaterialPoint->extraUniforms[1].offsets["uLightRadius"] = 28;
		deferredMaterialPoint->extraUniforms[1].offsets["uLightColor"] = 32;
		deferredMaterialPoint->extraUniforms[1].offsets["uNearFar"] = 48;
		deferredMaterialPoint->extraUniforms[1].offsets["uPointDepthsMVP"] = 64;
		deferredMaterialPoint->extraUniforms[1].offsets["uPCFTexelSize"] = 192;
		deferredMaterialPoint->extraUniforms[1].offsets["uHaveShadowmap"] = 196;
		// Fills the float std140 already left free here by the vec4's
		// alignment below, so the block size is unchanged.
		deferredMaterialPoint->extraUniforms[1].offsets["uShadowBias"] = 200;
		// vec4 -> next 16-byte boundary after 204.
		deferredMaterialPoint->extraUniforms[1].offsets["uVolumetricParams"] = 208;

		// FrontFace on BOTH backends - culling the light volume's *near*
		// faces so the far hemisphere is what covers the screen, the
		// standard "camera may be inside the light volume" deferred
		// technique. This used to be `IsVulkan() ? BackFace : FrontFace`,
		// justified as the two rasterizers disagreeing on this sphere's
		// winding. They don't: with the camera *outside* the proxy the
		// near and far hemispheres have the identical screen silhouette,
		// so either value produces a pixel-identical image - which is why
		// the inversion survived every previous verification pass (all of
		// which used a camera well outside the proxy). The two only differ
		// once the camera is *inside* the proxy sphere, where every
		// visible face is back-facing: GL kept them, Vulkan culled them
		// all and the light silently vanished. That happens for every
		// camera distance between the light's radius (where the
		// fullscreen-quad substitution below stops) and the proxy mesh's
		// own g(f(radius)) = ~1.41x radius scale - a real, wide dead band
		// (100..141 units for a radius-100 light) reported as "the point
		// light disappears at certain distances". Verified by capture on
		// both backends at 30/60/100/150/200/300/500 units.
		deferredMaterialPoint->SetCullFace(CullFace::FrontFace);
		deferredMaterialPoint->DisableDepthTest();
		deferredMaterialPoint->DisableDepthWrite();
		deferredMaterialPoint->EnableBlending();
		deferredMaterialPoint->BlendingEquation(BlendEq::Add);
		deferredMaterialPoint->BlendingFunction(BlendFunc::One, BlendFunc::One);

		spotPosHandle = deferredMaterialSpot->AddUniform(Uniform("uLightPosition", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
		spotDirHandle = deferredMaterialSpot->AddUniform(Uniform("uLightDirection", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
		spotRadiusHandle = deferredMaterialSpot->AddUniform(Uniform("uLightRadius", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		spotOutterHandle = deferredMaterialSpot->AddUniform(Uniform("uOutterCone", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		spotInnerHandle = deferredMaterialSpot->AddUniform(Uniform("uInnerCone", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		spotColorHandle = deferredMaterialSpot->AddUniform(Uniform("uLightColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		spotShadowHandle = deferredMaterialSpot->AddUniform(Uniform("uShadowMap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		spotShadowPCFTexelHandle = deferredMaterialSpot->AddUniform(Uniform("uPCFTexelSize", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		spotShadowDepthsMVPHandle = deferredMaterialSpot->AddUniform(Uniform("uSpotDepthsMVP", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		spotHaveShadowHandle = deferredMaterialSpot->AddUniform(Uniform("uHaveShadowmap", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		spotVolumetricHandle = deferredMaterialSpot->AddUniform(Uniform("uVolumetricParams", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		deferredMaterialSpot->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		deferredMaterialSpot->AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		deferredMaterialSpot->AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		deferredMaterialSpot->AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialSpot->AddUniform(Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));
		// Pre-existing bug, found and fixed alongside the attribute_in one
		// in secondpassSpot.glsl: uMatProj (used for view-space position
		// reconstruction in getPosViewSpace()) was never registered here,
		// unlike Ambient/Directional's identical uMatProj registrations
		// above - meaning it was always read as an all-zero matrix,
		// dividing by zero in getPosViewSpace()'s uMatProj_local[0][0]/
		// [1][1]. Broken on GL today too, not introduced by this pass.
		deferredMaterialSpot->AddUniform(Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));
		// See deferredMaterialPoint's identical comment above - same
		// near-plane-clipping fix, same mechanism.
		spotUseFullscreenQuadHandle = deferredMaterialSpot->AddUniform(Uniform("uUseFullscreenQuad", Uniforms::DataUsage::Other, Uniforms::DataType::Float));

		// See IMaterial.h's comment on extraUniforms[2] - two separate
		// blocks matching secondpassSpot.glsl's SpotVertParams (VERTEX
		// stage, binding 34) and SpotFragParams (FRAGMENT stage, binding
		// 39) exactly (see secondpassPoint.glsl's comment on why it's two
		// blocks, not one combined block used by both stages).
		deferredMaterialSpot->extraUniforms[0].binding = 34;
		deferredMaterialSpot->extraUniforms[0].blockName = "SpotVertParams";
		deferredMaterialSpot->extraUniforms[0].size = 196;
		deferredMaterialSpot->extraUniforms[0].scratch.resize(deferredMaterialSpot->extraUniforms[0].size, 0);
		deferredMaterialSpot->extraUniforms[0].offsets["uProjectionMatrix"] = 0;
		deferredMaterialSpot->extraUniforms[0].offsets["uViewMatrix"] = 64;
		deferredMaterialSpot->extraUniforms[0].offsets["uModelMatrix"] = 128;
		deferredMaterialSpot->extraUniforms[0].offsets["uUseFullscreenQuad"] = 192;

		deferredMaterialSpot->extraUniforms[1].binding = 39;
		deferredMaterialSpot->extraUniforms[1].blockName = "SpotFragParams";
		deferredMaterialSpot->extraUniforms[1].size = 256;
		deferredMaterialSpot->extraUniforms[1].scratch.resize(deferredMaterialSpot->extraUniforms[1].size, 0);
		deferredMaterialSpot->extraUniforms[1].offsets["uScreenDimensions"] = 0;
		deferredMaterialSpot->extraUniforms[1].offsets["uLightPosition"] = 16;
		deferredMaterialSpot->extraUniforms[1].offsets["uLightDirection"] = 32;
		deferredMaterialSpot->extraUniforms[1].offsets["uLightRadius"] = 44;
		deferredMaterialSpot->extraUniforms[1].offsets["uOutterCone"] = 48;
		deferredMaterialSpot->extraUniforms[1].offsets["uInnerCone"] = 52;
		deferredMaterialSpot->extraUniforms[1].offsets["uLightColor"] = 64;
		deferredMaterialSpot->extraUniforms[1].offsets["uNearFar"] = 80;
		deferredMaterialSpot->extraUniforms[1].offsets["uMatProj"] = 96;
		deferredMaterialSpot->extraUniforms[1].offsets["uSpotDepthsMVP"] = 160;
		deferredMaterialSpot->extraUniforms[1].offsets["uPCFTexelSize"] = 224;
		deferredMaterialSpot->extraUniforms[1].offsets["uHaveShadowmap"] = 228;
		// vec4 -> next 16-byte boundary after 232.
		deferredMaterialSpot->extraUniforms[1].offsets["uVolumetricParams"] = 240;

		// See deferredMaterialPoint's comment above - same Sphere-primitive
		// light volume, same inside-the-proxy dead band the old IsVulkan()
		// inversion caused.
		deferredMaterialSpot->SetCullFace(CullFace::FrontFace);
		deferredMaterialSpot->DisableDepthTest();
		deferredMaterialSpot->DisableDepthWrite();
		deferredMaterialSpot->EnableBlending();
		deferredMaterialSpot->BlendingEquation(BlendEq::Add);
		deferredMaterialSpot->BlendingFunction(BlendFunc::One, BlendFunc::One);

		// Light Volume
		quadHandle = std::make_shared<Plane>(1, 1);
		directionalLight = new RenderingComponent(quadHandle);
		// This mesh's own default Material is never actually used for
		// drawing (every real draw call passes deferredMaterialAmbient/
		// Directional/Point/Spot/deferredLastPass explicitly as an
		// override), but IRenderer::RenderObject() falls back to *this*
		// material's cull face whenever it differs from the override's
		// (see its own comment on effectiveCullFace) - left at the
		// RenderingComponent default (BackFace) that fallback silently
		// discarded the quad on every backend where SetCullFaceMode()
		// is not a no-op (GL was saved only by winding luck, Vulkan by
		// SetCullFaceMode() being baked into the pipeline instead - see
		// deferredMaterialPoint/Spot's identical temporary DoubleSided
		// toggle a few hundred lines below, which relies on this same
		// mesh already reading DoubleSided here to actually take effect).
		directionalLight->GetMeshes()[0]->Material->SetCullFace(CullFace::DoubleSided);

		sphereHandle = std::make_shared<Sphere>(1, 6, 4);
		pointLight = new RenderingComponent(sphereHandle);
		// This mesh's own default Material is never actually used for
		// drawing (every real draw call passes deferredMaterialPoint/Spot
		// explicitly as an override - see the light-rendering loop below)
		// but kept consistent with them regardless, matching their
		// identical CullFace fix/comment above.
		pointLight->GetMeshes()[0]->Material->SetCullFace(CullFace::FrontFace);
	}

	void DeferredRenderer::Resize(const uint32 &Width, const uint32 &Height)
	{
		// Nothing below has anything to do when the size has not actually
		// changed, and all of it is expensive: a full device->WaitIdle()
		// stall, a bind+clear of previousFrameFBO, and a mip-chain
		// regeneration. That matters because this is not only called on a
		// real resize - SceneEditor::ShowViewport() calls it every single
		// frame (unconditionally, the same way it calls ResetViewPort()/
		// SetViewPort()), so the editor paid all three every frame at a
		// measured median of ~7-8ms, about half a 60Hz frame budget. Worse
		// than the stall, the unconditional clear below threw away
		// previousFrameColorTexture - the previous frame's composite, which
		// is exactly what SSR reprojects from - so the editor's Deferred
		// viewport could never accumulate a frame of SSR history at all.
		//
		// IRenderer::Resize() still runs on every call: it is pure CPU
		// bookkeeping (Width/Height, the non-custom viewport extent) plus
		// Reset()'s depth-state defaults, and the per-frame caller has
		// always got that side effect - ForwardRenderer, which does not
		// override this, gets it on every frame too. Only the GPU work
		// below is gated.
		if (this->Width == Width && this->Height == Height)
		{
			IRenderer::Resize(Width, Height);
			return;
		}

		// Must run before ANY of the resource-destroying resizes below,
		// not just before the offscreen clear that follows them. A resize
		// can be delivered between frames while the previous frame's GPU
		// submission is still in flight - frameFence only gets waited on
		// at the top of the *next* BeginFrame()/DrawFrame() call, not
		// immediately after the last EndFrame(), so lastPassFBO->Resize()/
		// previousFrameFBO->Resize() (which destroy+recreate the
		// underlying VkImage, and any pipeline/sampler/descriptor still
		// referencing it) can race that still-in-flight submission.
		// Originally placed after these two Resize() calls - looked
		// correct (fixed the resize hang) but still let a real, distinct
		// bug through: VUID-vkDestroyPipeline-pipeline-00765 and
		// VUID-vkDestroySampler-sampler-01082 firing on SSRTest resize
		// under validation layers, from destroying resources the previous
		// frame's still-in-flight command buffer was using.
		device->WaitIdle();
		IRenderer::Resize(Width, Height);
		lastPassFBO->Resize(Width, Height);
		{
			const bool halfAO = g_ssaoHalfResolution && Width >= 64 && Height >= 64;
			ssaoFBO->Resize(halfAO ? Width / 2 : Width, halfAO ? Height / 2 : Height);
			ssaoBlurFBO->Resize(halfAO ? Width / 2 : Width, halfAO ? Height / 2 : Height);
		}
		// Resizing recreates previousFrameColorTexture's underlying image
		// (same "resize destroys+recreates the VkImage" behavior as any
		// other Vulkan texture - see Texture::Resize()), which puts it
		// right back in an undefined-content state - the one-time
		// dummyShadowsWarmedUp-gated clear in RenderScene() only ever
		// fires once and won't catch this. Re-clear unconditionally here
		// instead of trying to track a second warm-up flag.
		if (ssrOutFBO) ssrOutFBO->Resize(Width, Height);
		previousFrameFBO->Resize(Width, Height);
		previousFrameFBO->Bind();
		device->SetClearColor(Vec4(0.f, 0.f, 0.f, 0.f));
		device->Clear(device->TranslateBufferBit(Buffer_Bit::Color));
		previousFrameFBO->UnBind();
		// Resize() also reallocates the mip chain at the new size (empty/
		// undefined beyond level 0's clear above) - regenerate now so a
		// blurred (rough-surface) SSR sample taken before the next real
		// per-frame UpdateMipmap() (end of RenderScene()) doesn't read
		// garbage from an unpopulated higher mip.
		previousFrameColorTexture->UpdateMipmap();
	}

	DeferredRenderer::~DeferredRenderer()
	{
		// Everything below is GPU-backed (FBOs, textures, materials and the
		// pipelines cached against them) and the last submitted frame can
		// still be in flight here - the frame fence is only ever waited on
		// at the top of the *next* BeginFrame(), which will never come.
		// Same reasoning as Resize()'s leading WaitIdle above; without it
		// quitting is an intermittent crash rather than a clean exit.
		device->WaitIdle();
		delete lastPassFBO;
		delete colorTexture;
		delete ssrOutFBO;
		delete ssrOutTexture;
		delete previousFrameFBO;
		delete previousFrameColorTexture;
		delete forwardDepthTexture;
		delete dummyShadow2D;
		delete dummyShadowCube;
		delete shadowMaterial;
		delete shadowSkinnedMaterial;
		sphereHandle.reset();
		quadHandle.reset();
		delete deferredLastPass;
		delete deferredMaterialAmbient;
		delete deferredMaterialDirectional;
		delete deferredMaterialPoint;
		delete deferredMaterialSpot;
		delete deferredSSAO;
		delete deferredSSAOBlur;
		delete ssaoFBO;
		delete ssaoBlurFBO;
		delete ssaoTexture;
		delete ssaoBlurTexture;
		delete ssaoWhite;
		delete directionalLight;
		delete pointLight;
	}

	// The programs the G-buffer pass will draw this material with: its
	// DEFERRED_GBUFFER sibling (see RenderScene's own swap, which made it the
	// first time the material was drawn - in the middle of a game, a frame
	// held up for as long as the program took).
	void DeferredRenderer::MaterialListed(RenderingMesh* mesh, IMaterial* material)
	{
		static const uint32 kLit = ShaderUsage::Diffuse | ShaderUsage::CellShading | ShaderUsage::PBR;
		if (material == NULL || mesh == NULL || material->IsTransparent()) return;
		if (GenericShaderMaterial* gsm = dynamic_cast<GenericShaderMaterial*>(material))
		{
			const bool sibling = !gsm->IsCompiledForGBuffer() && (gsm->GetOptions() & kLit) != 0;
			if (sibling)
			{
				gsm->UseGBufferProgramForNextDraw();
				gsm->RestoreOwnProgram();
			}
			// (and the one it is drawn with when enough of the same thing are in
			// view to be drawn as one: that many came into view in the middle
			// of a game, never while it loaded)
			if (IsAutoInstancing() && AutoInstanceEligible(mesh))
			{
				gsm->UseVariantProgramForNextDraw(ShaderUsage::InstancedRendering | (sibling ? ShaderUsage::DeferredRenderer_Gbuffer : 0));
				gsm->RestoreOwnProgram();
			}
			return;
		}
		if (typeid(*material) == typeid(CustomShaderMaterial))
		{
			CustomShaderMaterial* csm = static_cast<CustomShaderMaterial*>(material);
			if (csm->UseVariantForNextDraw(true, mesh->SkinningBones.size() > 0 || !mesh->MapBoneIDs.empty())) csm->RestoreOwnProgram();
		}
	}

	// What is itself under the ground - in a cave, a cellar dug into a hill - is not
	// hidden by the ground's horizon: the horizon is made from the heights of the
	// surface, and says only that there is hill between the eye and that place, which
	// of a thing inside the hill is always true. (From a cave's mouth, nothing in the
	// cave was drawn.) Asked only of what the horizon would hide, and kept by its place
	// in the list until it moves: the height of the ground is not looked up every frame.
	static bool UnderTheGround(SceneGraph* scene, const size_t k, const Vec4 &sphere, const size_t count)
	{
		static thread_local std::vector<Vec4> at;
		static thread_local std::vector<uint8> known;         // 0 not asked, 1 above, 2 under
		if (known.size() != count) { known.assign(count, 0); at.assign(count, Vec4()); }
		if (known[k] != 0 && at[k].x == sphere.x && at[k].y == sphere.y && at[k].z == sphere.z) return known[k] == 2;
		f32 ground = 0.f;
		const bool under = TerrainEditor::HeightAt(scene, sphere.x, sphere.z, ground) && sphere.y < ground - 0.3f;
		at[k] = sphere;
		known[k] = under ? 2 : 1;
		return under;
	}

	void DeferredRenderer::RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene)
	{
		PYROS_PROFILE_SCOPE("Deferred.RenderScene");
		// (see IRenderer::FinishBeside: on every way out of here)
		struct Join { IRenderer* r; ~Join() { r->FinishBeside(); } } join = { this };

		// See DeferredRenderer.h's comment on dummyShadowsWarmedUp - a
		// one-time, contentless render-pass begin/end for each dummy
		// shadow texture, run inside a real frame (unlike the
		// constructor) so it leaves them in a real, sampleable layout
		// before any light ever binds one.
		if (!dummyShadowsWarmedUp)
		{
			FrameBuffer warmup2D;
			warmup2D.Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, dummyShadow2D);
			warmup2D.Bind();
			warmup2D.UnBind();

			// A cube texture is 6 separate layers/subresources - each one
			// only leaves VK_IMAGE_LAYOUT_UNDEFINED once *its own* face is
			// attached and rendered into, same as real point-light shadow
			// rendering's own per-face loop (IRenderer.cpp's
			// RenderingPointShadowFace block) - a single warm-up FBO
			// attaching just CubemapPositive_X (face/layer 0) only ever
			// transitions that one face, found via VUID-vkCmdDraw-None-09600
			// still firing for layers 1-5 after the single-face version of
			// this fix.
			for (uint32 face = 0; face < 6; face++)
			{
				FrameBuffer warmupCubeFace;
				// Colour, not depth: dummyShadowCube is R32F now, matching
				// the real point-light shadow cube map it stands in for
				// (see PointLight::EnableCastShadows). Attaching an R32F
				// image as a depth-stencil attachment builds a render pass
				// declaring R32_SFLOAT as its depth format, which MoltenVK
				// runs anyway but validation rejects outright
				// (VUID-VkSubpassDescription-pDepthStencilAttachment-02650,
				// and VUID-VkFramebufferCreateInfo-pAttachments-02633 for
				// the usage flags that follow). Six of these warm-ups ran
				// per DeferredRenderer, which is where the bulk of demo 4's
				// validation errors came from.
				warmupCubeFace.Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::CubemapPositive_X + face, dummyShadowCube);
				warmupCubeFace.Bind();
				warmupCubeFace.UnBind();
			}

			// previousFrameColorTexture's real content is undefined at
			// creation on both backends (glTexImage2D(NULL) doesn't
			// guarantee zero-fill, and a fresh VkImage starts in
			// VK_IMAGE_LAYOUT_UNDEFINED) - same "needs a real render-pass
			// before first use" problem as the dummy shadow textures
			// above, so warmed up alongside them. An explicit Clear()
			// (not just Bind()/UnBind()) since GL has no render-pass-
			// implied clear the way Vulkan's offscreen path does.
			previousFrameFBO->Bind();
			device->SetClearColor(Vec4(0.f, 0.f, 0.f, 0.f));
			device->Clear(device->TranslateBufferBit(Buffer_Bit::Color));
			previousFrameFBO->UnBind();
			// See Resize()'s identical comment - same reasoning.
			previousFrameColorTexture->UpdateMipmap();

			dummyShadowsWarmedUp = true;
		}

		// Initialize Renderer
		InitRender();

		// Get Lights List
		std::vector<IComponent*> lcomps = ILightComponent::GetLightsOnScene(Scene);
		PublishLightsToSmoke(lcomps);

		// Save Time
		Timer = Scene->GetTime();

		// First Pass

		// Save Values for Cache
		// Saves Scene
		this->Scene = Scene;

		// Saves Camera
		this->Camera = Camera;
		this->CameraPosition = this->Camera->GetWorldPosition();

		// Saves Projection
		this->projection = projection;
		this->projectionValid = true;
		smallCullFactor = 1.f;

		// Universal Cache
		// Shift current -> previous before overwriting, matching
		// ForwardRenderer::RenderScene()'s identical pattern - real,
		// necessary plumbing for material-aware SSR's reprojection
		// (deferredLastPass samples uPrvProjectionMatrix/uPrvViewMatrix),
		// not previously needed by anything DeferredRenderer itself drew.
		PrvProjectionMatrix = ProjectionMatrix;
		PrvViewMatrix = ViewMatrix;
		unjitteredProjectionMatrix = projection.m;
		ProjectionMatrix = ScenePassProjection(projection);
		NearFarPlane = Vec2(projection.Near, projection.Far);

		// View Matrix and Position
		ViewMatrix = Camera->GetWorldTransformation().Inverse();
		CameraPosition = Camera->GetWorldPosition();

		// Update Culling
		UpdateCulling(ProjectionMatrix*ViewMatrix);

		// Flags
		ViewMatrixInverseIsDirty = true;
		ProjectionMatrixInverseIsDirty = true;
		ViewProjectionMatrixIsDirty = true;

		// Real, pre-existing bug found chasing an unrelated black-screen
		// report: unlike ForwardRenderer::RenderScene() (see its identical
		// comment on this exact call), DeferredRenderer never called
		// device->BeginFrame()/EndFrame() at all - on GL both are no-ops so
		// this was invisible, but on Vulkan there is exactly ONE shared
		// per-frame command buffer (VulkanRenderDevice::frameCommandBuffer,
		// set as activeCommandBuffer inside BeginFrame()) that every
		// vkCmdBeginRenderPass in the frame - the G-buffer pass, the
		// lastPassFBO lighting-accumulation pass, AND the final swapchain
		// blit - records into. With BeginFrame() never called at all,
		// nothing in this whole function had a valid open command buffer
		// to record into from the very first FBO->Bind() below. Must be
		// called here, before any of that starts (matching
		// ForwardRenderer's placement right after DrawBackground() sets
		// the clear color) - NOT just wrapped around the final blit, which
		// was an earlier, incomplete attempt at this same fix: by the time
		// that ran, the G-buffer/lighting passes had already tried to
		// record into a command buffer that didn't exist yet.
		// isMainSwapchainPass gating matches ForwardRenderer, for the same
		// reason: don't hijack the active command buffer if this
		// RenderScene() call is itself targeting a caller-bound offscreen
		// FBO (e.g. a reflection pass) rather than the real swapchain.
		// And only end a frame this call opened - see ForwardRenderer's
		// identical ownFrame for what ending someone else's frame did.
		bool isMainSwapchainPass = device->GetCurrentRenderTarget() == 0;
		const bool ownFrame = isMainSwapchainPass && !device->IsFrameInProgress();

		if (ownFrame)
			device->BeginFrame();

		// Clear the framebuffer the CALLER left bound, before this renderer
		// binds any of its own.
		//
		// ForwardRenderer renders straight into that framebuffer and clears
		// it on the way past. This one never does: it binds the G-buffer
		// immediately below, then lastPassFBO, and only composites into the
		// caller's target at the very end - so nothing ever cleared it. On
		// Vulkan and Metal that stayed invisible, because binding a render
		// target there carries an implicit clear (the same asymmetry
		// PostEffectsManager::drawEffect documents from the other side). On
		// GL and WebGL2 it does not, and the editor viewport composited every
		// frame on top of the last one until it saturated to white - which
		// reads as "no clear between frames", not as a deferred bug.
		//
		// DrawBackground() first: it only sets the clear colour, it draws
		// nothing, and this clear should use the scene's own background
		// instead of whatever the last renderer left on the device.
		DrawBackground();
		ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
		ClearScreen();

		// Bind Frame Buffer
		FrameProfiler::Instance().Begin("Deferred.GBuffer");
		FBO->Bind();

		// Set ViewPort
		viewPortEndX = Width;
		viewPortEndY = Height;
		_SetViewPort(viewPortStartX, viewPortStartY, viewPortEndX, viewPortEndY);

		ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
		ClearDepthBuffer();
		ClearScreen();

		// Draw Background
		DrawBackground();

		// Render Scene with Objects Material
		// Visible opaque meshes first, then the draws - DrawWithAutoInstancing
		// batches the ones that share geometry and material content.
		// (what is in the view, from the frame's cull list - IRenderer::BuildCullList)
		std::vector<RenderingMesh*> visible;
		visible.reserve(rmesh.size());
		std::vector<uint32> visibleAt;
		visibleAt.reserve(rmesh.size());
		// What the ground hides from the camera is left out of the picture (it
		// still casts its shadow: the shadow maps have their own lists).
		bool occluding = false;
		{
			PYROS_PROFILE_SCOPE("Occlusion.Update");
			occluding = terrainOcclusion.Update(Scene, Camera->GetWorldPosition());
		}
		uint32 occluded = 0;
		if (cullFlags.size() != rmesh.size()) BuildCullList();
		{
			const uint8 need = CullOwner | CullComponentActive | CullMeshActive;
			for (size_t k = 0; k < rmesh.size(); k++)
			{
				const uint8 f = cullFlags[k];
				if ((f & need) != need || (f & CullTransparent)) continue;
				if (!(f & CullTested) || CullListTest(k))
				{
					// (and not what the ground hides: behind a hill from here)
					if (occluding && cullSphere[k].w > 0.f && terrainOcclusion.Hidden(Vec3(cullSphere[k].x, cullSphere[k].y, cullSphere[k].z), cullSphere[k].w)
						&& !UnderTheGround(Scene, k, cullSphere[k], rmesh.size()))
					{
						// PYROS_VERIFY_OCCLUSION=1: every "hidden" held against the ground
						// itself - a line from the eye to the sphere's top, and to its top
						// at either side, has to go into the ground on the way.
						static const bool verify = std::getenv("PYROS_VERIFY_OCCLUSION") != NULL;
						if (verify)
						{
							static uint64 asked = 0, wrong = 0;
							const Vec3 eyeNow = Camera->GetWorldPosition();
							const Vec4 &s = cullSphere[k];
							const Vec3 c(s.x, s.y, s.z);
							Vec3 side(-(c.z - eyeNow.z), 0.f, c.x - eyeNow.x);
							const f32 sl = sqrtf(side.x * side.x + side.z * side.z);
							if (sl > 1e-3f) side = side * (s.w / sl);
							const Vec3 tops[3] = { c + Vec3(0.f, s.w, 0.f), c + Vec3(0.f, s.w, 0.f) + side, c + Vec3(0.f, s.w, 0.f) - side };
							bool seen = false;
							for (int t = 0; t < 3; t++) if (!TerrainOcclusion::GroundBetween(Scene, eyeNow, tops[t])) seen = true;
							asked++;
							if (seen && ++wrong <= 20) fprintf(stderr, "[occlusion] WRONG: %s hidden, but a line to its top is clear (%.0f m off, radius %.1f)\n", rmesh[k]->renderingComponent->GetOwner()->GetName().c_str(), sqrtf(eyeNow.distanceSQR(c)), s.w);
							if (asked % 200000 == 0) fprintf(stderr, "[occlusion] %llu hidden checked, %llu wrong\n", (unsigned long long)asked, (unsigned long long)wrong);
						}
						occluded++; continue;
					}
					visible.push_back(rmesh[k]); visibleAt.push_back((uint32)k);
				}
			}
		}
		// Nearest first. What is drawn first hides what is drawn after it, and
		// a pixel that is hidden is not shaded: in the order things happened to
		// be added to the scene, a field of grass seen along the ground was
		// shaded many times over, far blades first and near ones on top.
		FrameProfiler::Instance().Counter("Occlusion.Hidden", (f64)occluded);
		FrameProfiler::Instance().Counter("Occlusion.Drawn", (f64)visible.size());
		if (g_gbufferNearestFirst && visible.size() > 1)
		{
			const Vec3 eye = Camera->GetWorldPosition();
			std::vector<std::pair<f32, uint32> > order(visible.size());
			for (size_t k = 0; k < visible.size(); k++)
			{
				const Vec4 &sp = cullSphere[visibleAt[k]];
				const f32 dx = sp.x - eye.x, dy = sp.y - eye.y, dz = sp.z - eye.z;
				order[k] = std::make_pair(sqrtf(dx * dx + dy * dy + dz * dz) - sp.w, (uint32)k);
			}
			std::sort(order.begin(), order.end());
			std::vector<RenderingMesh*> sorted(visible.size());
			for (size_t k = 0; k < order.size(); k++) sorted[k] = visible[order[k].second];
			visible.swap(sorted);
		}

		static const uint32 kLitUsageMask = ShaderUsage::Diffuse | ShaderUsage::CellShading | ShaderUsage::PBR;
		DrawWithAutoInstancing(visible, NULL,
			[&](RenderingMesh* mesh, uint32)
			{
				// Only materials that actually run PyrosShader.glsl's
				// lit path (DIFFUSE/CELLSHADING/PBR) need swapping -
				// their G-buffer branch reads gbuffer_normals, which
				// the vertex stage only computes from aNormal when
				// one of those is set (see PyrosShader.glsl). An
				// unlit Color-only material (e.g. the editor's own
				// grid helper, drawn through this same per-object
				// loop) has no aNormal in its own mesh at all - the
				// vertex shader for ITS G-buffer variant would need
				// one anyway (DEFERRED_GBUFFER's gbuffer_normals
				// write isn't itself gated on DIFFUSE), so swapping
				// it in builds a pipeline whose vertex input state
				// is missing a location the shader declares
				// (VUID-VkGraphicsPipelineCreateInfo-Input-07904) -
				// found via exactly this: the grid's own material
				// failing pipeline creation once this swap covered
				// every GenericShaderMaterial instead of just lit
				// ones. An unlit material was never miscounted as
				// "ambient" by the composite pass to begin with (its
				// FragColor.w is just opacity either way), so
				// leaving it on its own Forward-only program changes
				// nothing it was already relying on.
				IMaterial* mat = mesh->Material.get();
				GenericShaderMaterial* gsm = dynamic_cast<GenericShaderMaterial*>(mat);
				const bool needsGBufferSwap = gsm && !gsm->IsCompiledForGBuffer() && (gsm->GetOptions() & kLitUsageMask) != 0;
				if (needsGBufferSwap)
					gsm->UseGBufferProgramForNextDraw();

				// Scoped to exactly CustomShaderMaterial (typeid, not
				// dynamic_cast) rather than any subclass - subclasses
				// like ParticleMaterial/CustomMaterialExample hand-
				// assign extraUniforms[] themselves for an explicit
				// hand-authored UBO, which this swap's std140 auto-
				// layout probe (GetAutoUniformBlockLayout) doesn't
				// know how to reproduce for a second, G-buffer-only
				// program - see CustomShaderMaterial::
				// UseGBufferProgramForNextDraw()'s comment for what
				// this fixes for the base class.
				CustomShaderMaterial* csm = (typeid(*mat) == typeid(CustomShaderMaterial)) ? static_cast<CustomShaderMaterial*>(mat) : nullptr;
				const bool usedCustomGBufferSwap = csm && csm->UseVariantForNextDraw(true, mesh->SkinningBones.size() > 0);

				RenderObject(mesh, mesh->renderingComponent->GetOwner(), mesh->Material.get());

				if (needsGBufferSwap)
					gsm->RestoreOwnProgram();
				if (usedCustomGBufferSwap)
					csm->RestoreOwnProgram();
			},
			[&](RenderingMesh* batchMesh, uint32)
			{
				// The instanced sibling of whatever the single draw would
				// have used: plus DEFERRED_GBUFFER exactly when it swaps.
				GenericShaderMaterial* gsm = static_cast<GenericShaderMaterial*>(batchMesh->Material.get());
				const bool gbuffer = !gsm->IsCompiledForGBuffer() && (gsm->GetOptions() & kLitUsageMask) != 0;
				gsm->UseVariantProgramForNextDraw(ShaderUsage::InstancedRendering | (gbuffer ? ShaderUsage::DeferredRenderer_Gbuffer : 0));
				RenderObject(batchMesh, batchMesh->renderingComponent->GetOwner(), gsm);
				gsm->RestoreOwnProgram();
			});

		// End Rendering
		EndRender();

		// Unbind FrameBuffer
		FBO->UnBind();
		FrameProfiler::Instance().End();

		// Refresh forwardDepthTexture with this frame's real G-buffer
		// depth - see DeferredRenderer.h's comment on forwardDepthTexture
		// for why lastPassFBO can't just alias FBO's depth attachment
		// directly. Must happen after FBO->UnBind() (source depth values
		// are only final once the G-buffer pass has finished writing them)
		// and before lastPassFBO->Bind() (whose lighting materials sample
		// tDepth from FBO's original depth texture, unaffected by this
		// copy, while its own attachment set gets forwardDepthTexture).
		device->CopyDepthTexture(FBO->GetAttachments()[0]->TexturePTR->GetBindID(), forwardDepthTexture->GetBindID(), Width, Height);

		// Drain before the lighting pass samples what the G-buffer pass above
		// just wrote. Without this, on macOS/Vulkan (MoltenVK) roughly 44% of
		// cold starts had the second pass read the G-buffer back as the clear
		// colour (0.2) or NaN instead of what was rendered into it. Every
		// secondpass*.glsl rejects sky with `if (tDepth >= 1.0) discard`, so
		// neither value discards anything: the ambient pass and every light
		// shaded the whole screen and this RGBA16F target saturated to a white
		// viewport. Measured 7 bad in 16 cold starts without, 0 in 48 with.
		// The state latches in the first frames and then sticks for the life
		// of the renderer, which is why recreating it (a Forward<->Deferred
		// toggle) or a viewport resize appeared to "fix" it.
		//
		// Cost, measured settled on the editor's E2E viewport in 300-frame
		// blocks: the wait is 2.2-3.1ms and the frame interval goes 8.75ms ->
		// ~10.1ms. Real, but both are inside the 16.7ms budget and macOS
		// presents at a hard 60Hz through CAMetalLayer regardless (see
		// WaitAllFrameFences()'s notes), so presented pacing does not change.
		//
		// CONTAINMENT, not a root-cause fix - the root cause is still open,
		// and everything below was tried and left the failure rate unchanged,
		// so do not re-try them:
		//   - it is not the descriptors: a good and a bad run trace identical
		//     binding/unit/VkImageView through SendUniformInt;
		//   - it is not the sampler ring wrapping onto a live set (zero wraps,
		//     zero short rings measured), nor the FBO's preserveDepth flag;
		//   - it is not the depth *format*. This looks exactly like the
		//     MoltenVK depth-sampling defect PointLight::EnableCastShadows()
		//     documents, but moving the sky test off tDepth entirely (onto
		//     length(tNormal.xyz)) still gave 9 bad in 16 - tNormal is equally
		//     unreliable, so the whole G-buffer read is, not one attachment;
		//   - it is not intra-command-buffer ordering: the G-buffer pass and
		//     this one are in the same command buffer and the same submission,
		//     and explicit barriers at the end of the offscreen render pass and
		//     on CopyDepthTexture's under-declared source masks both did
		//     nothing (that second one is a genuine defect worth fixing on its
		//     own merits, just not this);
		//   - it is not the offscreen semaphore chain, though that IS broken
		//     independently: EndFrame consumes offscreenChainSemaphore every
		//     frame, so by the next flush it is always VK_NULL_HANDLE and each
		//     offscreen submit goes out with no wait at all, contradicting the
		//     ordering its own header comment promises. Giving each slot a
		//     second semaphore so both consumers get one fixes that gap - and
		//     still left 10 bad in 16 here.
		// A drain at the *top* of RenderScene does not help either (9 bad in
		// 16); it only works in this position.
		// Hard submission boundary between the G-buffer pass above and the
		// lighting pass below. Both were being recorded into one offscreen
		// command buffer and submitted together, and on macOS/Vulkan
		// (MoltenVK) roughly 44% of cold editor starts - and ~12% of player
		// starts - then had the lighting pass read the G-buffer back as the
		// clear colour (0.2) or NaN instead of what had just been rendered
		// into it. Every secondpass*.glsl rejects sky with
		// `if (tDepth >= 1.0) discard`, so neither value discards anything:
		// the ambient pass and every light shaded the whole screen and this
		// RGBA16F target saturated to a white viewport. Splitting the
		// submission lets the backend's own submit ordering apply, which is
		// what the sampled read needed and what no in-command-buffer barrier
		// could provide.
		//
		// Depends on VulkanRenderDevice's offscreen semaphore chain actually
		// chaining - it did not until the doneForOffscreen semaphore was added
		// alongside it (EndFrame consumed the only signal every frame, so
		// every offscreen submit went out with no wait at all). The two go
		// together: neither fixes this alone.
		//
		// Measured on the editor's E2E viewport, 300-frame blocks, settled:
		// 0.10ms per call and a frame interval of 8.53-8.56ms against
		// 8.64-8.85ms unfixed - i.e. free. The first fix found for this was a
		// device->WaitIdle() here, which also works but costs 1.6-2.4ms and
		// pushes the frame interval to ~10.1ms because it drains the previous
		// frame and destroys CPU/GPU pipelining. Do not go back to it.
		//
		// What is *below* this is still unexplained: with both passes in one
		// submission, the descriptors, the sampler ring, the FBO layouts and
		// explicit barriers all measured identical between a good and a bad
		// run. Ruled out, do not re-test: the depth *format* (moving the sky
		// test onto tNormal reproduces just the same, so it is not the
		// MoltenVK depth-sampling defect PointLight::EnableCastShadows
		// documents), CopyDepthTexture's under-declared source masks, and a
		// warm-up-only drain (clean at 16/16, then 1 bad in 20 - it only
		// lowers the rate).
		device->FlushOffscreenWork();

		// Ambient occlusion, from the G-buffer the flush above just made
		// readable, into the texture the ambient pass below multiplies by.
		if (ssaoEnabled)
		{
			// Half the samples, turned a different way each frame, where the frames
			// are added together afterwards (temporal anti-aliasing): over a few
			// frames a pixel has looked along more directions than it did in one.
			f32 ssaoSamplesSent = ssaoSamples;
			if (g_ssaoTemporal)
			{
				static const f32 kTurn[8] = { 0.f, 0.5f, 0.25f, 0.75f, 0.125f, 0.625f, 0.375f, 0.875f };
				static uint32 frame = 0;
				ssaoSamplesSent = std::max(4.f, floorf(ssaoSamples * 0.5f)) + kTurn[(frame++) & 7] * 0.999f;
			}
			for (int m = 0; m < 2; m++)
			{
				ssaoHandles[m][0]->SetValue(&ssaoRadius);
				ssaoHandles[m][1]->SetValue(&ssaoStrength);
				ssaoHandles[m][2]->SetValue(&ssaoFalloff);
				ssaoHandles[m][3]->SetValue(&ssaoSamplesSent);
				ssaoHandles[m][4]->SetValue(&ssaoDirect);
			}
			GameObject go = GameObject();

			FrameProfiler::Instance().Begin("Deferred.SSAO");
			// At half the frame's size when asked (SetSSAOHalfResolution): a
			// quarter of the pixels for the sixteen depth reads each one costs,
			// and the blur after it. The lighting pass reads the result by
			// texture coordinate, so it is stretched back over the frame there.
			// For these two passes the renderer's size IS the smaller one - the
			// shaders work out where they are from it.
			const uint32 fullWidth = Width, fullHeight = Height;
			const bool halfAO = g_ssaoHalfResolution && Width >= 64 && Height >= 64;
			const uint32 aoWidth = halfAO ? (Width / 2) : Width, aoHeight = halfAO ? (Height / 2) : Height;
			if (ssaoTexture->GetWidth() != aoWidth || ssaoTexture->GetHeight() != aoHeight)
			{
				device->WaitIdle();
				ssaoFBO->Resize(aoWidth, aoHeight);
				ssaoBlurFBO->Resize(aoWidth, aoHeight);
			}
			Width = aoWidth; Height = aoHeight;
			_SetViewPort(0, 0, aoWidth, aoHeight);
			ssaoFBO->Bind();
			InitRender();
			GetGBufferAttachment(FrameBufferAttachmentFormat::Depth_Attachment)->Bind();
			GetGBufferAttachment(FrameBufferAttachmentFormat::Color_Attachment2)->Bind();
			RenderObject(directionalLight->GetMeshes()[0], &go, deferredSSAO);
			GetGBufferAttachment(FrameBufferAttachmentFormat::Color_Attachment2)->Unbind();
			GetGBufferAttachment(FrameBufferAttachmentFormat::Depth_Attachment)->Unbind();
			EndRender();
			ssaoFBO->UnBind();

			ssaoBlurFBO->Bind();
			InitRender();
			ssaoTexture->Bind();
			GetGBufferAttachment(FrameBufferAttachmentFormat::Depth_Attachment)->Bind();
			RenderObject(directionalLight->GetMeshes()[0], &go, deferredSSAOBlur);
			GetGBufferAttachment(FrameBufferAttachmentFormat::Depth_Attachment)->Unbind();
			ssaoTexture->Unbind();
			EndRender();
			ssaoBlurFBO->UnBind();
			Width = fullWidth; Height = fullHeight;
			_SetViewPort(0, 0, fullWidth, fullHeight);
			FrameProfiler::Instance().End();

			// Same boundary as above, for the same reason: the ambient
			// pass samples what these two passes just rendered.
			device->FlushOffscreenWork();
		}

		FrameProfiler::Instance().Begin("Deferred.Lighting");
		lastPassFBO->Bind();
		ClearBufferBit(Buffer_Bit::Color);
		ClearScreen();

		// Initialize Rendering
		InitRender();

		// Bind FBO Textures
		for (int i = 0;i<(int)FBO->GetAttachments().size();i++)
			FBO->GetAttachments()[i]->TexturePTR->Bind();

		// The AO texture takes unit 5, after the five G-buffer attachments,
		// for the ambient pass and every light pass. Each light's shadow map
		// is bound onto the next free unit, which this pushes up to 6.
		Texture *aoTexture = ssaoEnabled ? ssaoBlurTexture : ssaoWhite;
		aoTexture->Bind();

		// Ambient
		// The ambient light. Where there is a sun, the sun's pass lays it along
		// with its own (uAmbientToo): both read the whole G-buffer for every
		// pixel of the frame, and the target they add into has just been
		// cleared - one pass over it in place of two. With no sun, its own pass.
		bool sunLaysAmbient = false;
		{
			static const bool allowed = std::getenv("PYROS_NO_LIGHT_MERGE") == NULL;
			const bool merge = allowed && g_sunLaysAmbient;
			for (std::vector<IComponent*>::iterator i = lcomps.begin(); merge && i != lcomps.end() && !sunLaysAmbient; i++)
				if ((*i)->GetOwner() != NULL && ((ILightComponent*)(*i))->GetLightType() == LIGHT_TYPE::DIRECTIONAL) sunLaysAmbient = true;
		}
		if (!sunLaysAmbient)
		{
			GameObject go = GameObject();
			RenderObject(directionalLight->GetMeshes()[0], &go, deferredMaterialAmbient);
		}
		// End Ambient

		uint32 numberDir = 0, numberPoint = 0, numberSpot = 0;

		// Render Lights
		for (std::vector<IComponent*>::iterator i = lcomps.begin(); i != lcomps.end(); i++)
		{

			if ((*i)->GetOwner() != NULL)
			{
				switch (((ILightComponent*)(*i))->GetLightType())
				{
				case LIGHT_TYPE::POINT:
				{
					PointLight* p = (PointLight*)(*i);
					// One that reaches nothing in view is not drawn. Every point
					// light in the scene was - its uniforms sent, its volume
					// drawn - wherever it was and however dim: a village of two
					// hundred street lamps, off for the day, was two hundred
					// draws a frame and two milliseconds. (Not one that casts a
					// shadow: its place in the list of shadow matrices is by
					// count, and it is counted below.)
					{
						// (PYROS_NO_LIGHT_CULL=1 draws them all, to compare against)
						static const bool cullLights = std::getenv("PYROS_NO_LIGHT_CULL") == NULL;
						if (cullLights && !p->IsCastingShadows() && !LightAffectsView(p->GetOwner()->GetWorldPosition(), p->GetLightRadius()))
							break;
					}
					// Point Lights
					Vec3 pos = (ViewMatrix * Vec4(p->GetOwner()->GetWorldPosition(), 1.f)).xyz();
					pointPosHandle->SetValue(&pos);
					pointRadiusHandle->SetValue((void*)&p->GetLightRadius());
					Vec4 pointRadiance = p->GetLightRadiance();
					pointColorHandle->SetValue(&pointRadiance);
					// See ILightComponent::SetVolumetricScattering() - x
					// density, y anisotropy, z steps; density 0 makes the
					// shader skip the march entirely.
					Vec4 pointVolumetric(p->GetVolumetricScattering(), p->GetVolumetricAnisotropy(), (f32)p->GetVolumetricSteps(), 0.f);
					pointVolumetricHandle->SetValue(&pointVolumetric);
					// Pre-existing bug, found and fixed alongside the other
					// second-pass bugs above: defaulting to unit 0 when the
					// light doesn't cast a shadow makes uShadowMap (a
					// samplerCubeShadow) claim the same GL texture unit as
					// tDepth (a sampler2D, always bound there) - two
					// different sampler *types* on one unit fails
					// glValidateProgram's sampler check with
					// GL_INVALID_OPERATION at the next draw. 4 is the unit
					// a real shadow bind would land on anyway (right after
					// the 4 G-buffer attachments bound above) - harmless
					// when nothing real is bound there since the shader
					// only ever samples uShadowMap behind `uHaveShadowmap >
					// 0.0`.
					int shadowUnit = 4;
					float haveShadow = 0.f;
					if (p->IsCastingShadows())
					{
						f32 txl = p->GetShadowFilterPacked();
						f32 bias = p->GetShadowBiasScale();
						Matrix mvp[2];
						mvp[0] = PointShadowMatrix[numberPoint];
						mvp[1] = PointShadowMatrix[numberPoint+1];
						pointShadowPCFTexelHandle->SetValue((void*)&txl);
						pointShadowBiasHandle->SetValue((void*)&bias);
						pointShadowDepthsMVPHandle->SetValue(&mvp, 2);
						p->GetShadowMapTexture()->Bind();
						shadowUnit = Texture::GetLastBindedUnit();
						haveShadow = 1.f;
					}
					else
					{
						// See DeferredRenderer.h's comment on dummyShadowCube.
						dummyShadowCube->Bind();
						shadowUnit = Texture::GetLastBindedUnit();
					}
					pointShadowHandle->SetValue(&shadowUnit);
					pointHaveShadowHandle->SetValue(&haveShadow);

					// See DeferredRenderer.h/PointVertParams' comment on
					// uUseFullscreenQuad. Real, corrected test - the
					// original version of this check was `distance <
					// radius` (camera literally inside the sphere), which
					// only ever catches the narrowest case. What actually
					// causes near-plane clipping is the sphere's *near
					// edge* (distance-to-camera minus radius) crossing
					// the near clip plane - a real gap this check missed
					// even for a camera sitting just outside a light's
					// radius, which is by far the more common case for a
					// scene with many small lights (found via a real
					// report that DeferredRendering's 100 radius-0.5
					// lights still visibly cut in/out with camera pitch
					// alone - reproduced with the fullscreen-quad branch
					// forcibly disabled, which ruled out the substitution
					// mechanism itself and pointed straight back at this
					// threshold). A small multiple of the near plane
					// distance as margin, not the FOV-inflated g(f(radius))
					// the sphere's own *mesh scale* uses a few lines below
					// for a completely unrelated reason (mesh under-
					// coverage compensation, not a clipping-distance
					// threshold - an earlier version of this check reused
					// that by mistake instead).
					bool cameraInsideVolume = CameraPosition.distance(p->GetOwner()->GetWorldPosition()) - p->GetLightRadius() < NearFarPlane.x * 2.0f;
					float useFullscreenQuad = cameraInsideVolume ? 1.f : 0.f;
					pointUseFullscreenQuadHandle->SetValue(&useFullscreenQuad);
					{
						static const bool ltrace = (getenv("PYROS_LIGHT_TRACE") != NULL);
						if (ltrace)
						{
							char lb[320];
							snprintf(lb, sizeof(lb),
								"PointLight '%s' worldDist=%.2f radius=%.2f intensity=%.2f "
								"viewPos=(%.2f,%.2f,%.2f) near=%.3f quad=%d",
								p->GetOwner()->GetName().c_str(),
								CameraPosition.distance(p->GetOwner()->GetWorldPosition()),
								p->GetLightRadius(), p->GetLightIntensity(),
								pos.x, pos.y, pos.z, NearFarPlane.x, (int)cameraInsideVolume);
							echo(std::string(lb));
						}
					}

					if (cameraInsideVolume)
					{
						// The quad substitution above needs the quad
						// itself to actually reach the rasterizer -
						// deferredMaterialPoint's CullFace (Front/BackFace,
						// tuned for the *sphere's* winding, see its own
						// SetCullFace() comment) culls this quad away
						// entirely on one winding, silently zeroing every
						// point light's contribution the moment the camera
						// got within radius of any of them - found via a
						// real regression on DeferredPBRSpheres (lights
						// went flat/highlight-less again with a static,
						// close camera). DoubleSided here matches how
						// deferredMaterialAmbient/Directional already
						// render their own identical full-screen quad.
						deferredMaterialPoint->SetCullFace(CullFace::DoubleSided);
						RenderObject(directionalLight->GetMeshes()[0], p->GetOwner(), deferredMaterialPoint);
						deferredMaterialPoint->SetCullFace(CullFace::FrontFace);
					}
					else
					{
						// Set Scale
						f32 sc = g(f(p->GetLightRadius()));
						Matrix m; m.Scale(sc, sc, sc);
						pointLight->GetMeshes()[0]->Pivot = m;
						RenderObject(pointLight->GetMeshes()[0], p->GetOwner(), deferredMaterialPoint);
					}
					if (p->IsCastingShadows())
					{
						p->GetShadowMapTexture()->Unbind();
						// Two matrices per light, not one: IRenderer.cpp's
						// point block pushes the light's projection and then
						// its view, and the read above is
						// PointShadowMatrix[numberPoint] and [numberPoint+1].
						// Advancing by 1 gave the *second* shadow-casting
						// point light the first light's view matrix as its
						// projection and its own projection as its view -
						// every lookup nonsense, everything occluded, the
						// light black. Invisible with a single point light,
						// which is all any demo had. The forward path indexes
						// the same array as ShadowMap*2 / ShadowMap*2+1 and
						// was always right.
						numberPoint += 2;
					}
					else
					{
						dummyShadowCube->Unbind();
					}
				}
				break;
				case LIGHT_TYPE::SPOT:
				{
					SpotLight* s = (SpotLight*)(*i);
					// Same as the point case above. Conservative: the cone is
					// tested as the sphere that contains it.
					if (!LightAffectsView(s->GetOwner()->GetWorldPosition(), s->GetLightRadius()))
						break;
					// Spot Lights
					Vec3 pos = (ViewMatrix * Vec4(s->GetOwner()->GetWorldPosition(), 1.f)).xyz();
					Vec3 dir = (ViewMatrix * (s->GetOwner()->GetWorldTransformation() * Vec4(s->GetLightDirection(), 0.f))).xyz();
					spotPosHandle->SetValue(&pos);
					spotDirHandle->SetValue(&dir);
					spotRadiusHandle->SetValue((void*)&s->GetLightRadius());
					spotOutterHandle->SetValue((void*)&s->GetLightCosOutterCone());
					spotInnerHandle->SetValue((void*)&s->GetLightCosInnerCone());
					Vec4 spotRadiance = s->GetLightRadiance();
					spotColorHandle->SetValue(&spotRadiance);
					// See the identical block in the POINT case above.
					Vec4 spotVolumetric(s->GetVolumetricScattering(), s->GetVolumetricAnisotropy(), (f32)s->GetVolumetricSteps(), 0.f);
					spotVolumetricHandle->SetValue(&spotVolumetric);

					// See the identical fix in the POINT case above.
					int shadowUnit = 4;
					float haveShadow = 0.f;
					if (s->IsCastingShadows())
					{
						f32 txl = s->GetShadowFilterPacked();
						// numberSpot, not numberDir. This indexed the spot
						// shadow matrices with the *directional* light
						// counter while incrementing numberSpot below and
						// never reading it, so with no directional light in
						// the scene every shadow-casting spot sampled
						// SpotShadowMatrix[0] - each spot binding its own
						// shadow map but projecting it through the first
						// spot's view. One spot looked correct and the rest
						// threw hard-edged shadows at wrong angles in places
						// nothing was occluding, with no shadow where one
						// belonged. The point and directional cases above
						// and below both index with their own counter.
						Matrix mvp = SpotShadowMatrix[numberSpot];
						spotShadowPCFTexelHandle->SetValue(&txl);
						spotShadowDepthsMVPHandle->SetValue(&mvp);
						s->GetShadowMapTexture()->Bind();
						shadowUnit = Texture::GetLastBindedUnit();
						haveShadow = 1.f;
					}
					else
					{
						// See DeferredRenderer.h's comment on dummyShadow2D.
						dummyShadow2D->Bind();
						shadowUnit = Texture::GetLastBindedUnit();
					}
					spotShadowHandle->SetValue(&shadowUnit);
					spotHaveShadowHandle->SetValue(&haveShadow);

					// See deferredMaterialPoint's identical comments above -
					// same near-plane-clipping fix, same real-radius
					// threshold (not g(f(radius))), same mechanism.
					bool cameraInsideVolume = CameraPosition.distance(s->GetOwner()->GetWorldPosition()) - s->GetLightRadius() < NearFarPlane.x * 2.0f;
					float useFullscreenQuad = cameraInsideVolume ? 1.f : 0.f;
					spotUseFullscreenQuadHandle->SetValue(&useFullscreenQuad);

					if (cameraInsideVolume)
					{
						deferredMaterialSpot->SetCullFace(CullFace::DoubleSided);
						RenderObject(directionalLight->GetMeshes()[0], s->GetOwner(), deferredMaterialSpot);
						deferredMaterialSpot->SetCullFace(CullFace::FrontFace);
					}
					else
					{
						// Set Scale
						f32 sc = g(f(s->GetLightRadius()));
						Matrix m; m.Scale(sc, sc, sc);
						pointLight->GetMeshes()[0]->Pivot = m;
						RenderObject(pointLight->GetMeshes()[0], s->GetOwner(), deferredMaterialSpot);
					}

					if (s->IsCastingShadows())
					{
						s->GetShadowMapTexture()->Unbind();
						numberSpot++;
					}
					else
					{
						dummyShadow2D->Unbind();
					}
				}
				break;
				case LIGHT_TYPE::DIRECTIONAL:
				{
					DirectionalLight* d = (DirectionalLight*)(*i);
					// Directional Lights
					Vec3 dir = (ViewMatrix * (d->GetOwner()->GetWorldTransformation() * Vec4(d->GetLightDirection(), 0.f))).xyz().normalize();
					dirDirHandle->SetValue(&dir);
					Vec4 dirRadiance = d->GetLightRadiance();
					dirColorHandle->SetValue(&dirRadiance);
					// See the identical fix in the POINT case above.
					int shadowUnit = 4;
					float haveShadow = 0.f;
					if (d->IsCastingShadows())
					{
						// See ILightComponent::GetShadowFilterPacked().
						f32 txl = d->GetShadowFilterPacked();

						// Linear view distances - see the matching comment in
						// IRenderer::PreRender(). This copy used the raw,
						// untranslated projection, so on Vulkan it picked
						// cascades from the wrong depth convention.
						Vec4 ShadowFar = d->GetCascadeSplits();

						std::vector<Matrix> mvp;
						for (int j = 0; j < (int)d->GetNumberCascades(); j++)
							mvp.push_back(DirectionalShadowMatrix[numberDir+j]);

						dirShadowPCFTexelHandle->SetValue(&txl);
						dirShadowDepthsMVPHandle->SetValue(&mvp[0], d->GetNumberCascades());
						dirShadowFarHandle->SetValue(&ShadowFar);

						d->GetShadowMapTexture()->Bind();
						shadowUnit = Texture::GetLastBindedUnit();
						haveShadow = 1.f;
					}
					else
					{
						// See DeferredRenderer.h's comment on dummyShadow2D.
						dummyShadow2D->Bind();
						shadowUnit = Texture::GetLastBindedUnit();
					}
					dirShadowHandle->SetValue(&shadowUnit);
					dirHaveShadowHandle->SetValue(&haveShadow);
					// (the first sun drawn carries the ambient light; any after it,
					// only its own. Carrying it, the pass WRITES where the others
					// add: the target was cleared to the scene's background, which
					// is what the sky keeps, and under everything else the ambient
					// pass used to write over it.)
					const bool carriesAmbient = sunLaysAmbient;
					sunLaysAmbient = false;
					{
						const f32 ambientToo = carriesAmbient ? 1.f : 0.f;
						dirAmbientTooHandle->SetValue((void*)&ambientToo);
						deferredMaterialDirectional->BlendingFunction(BlendFunc::One, carriesAmbient ? BlendFunc::Zero : BlendFunc::One);
					}

					// The terrain's baked shadow: how high the sun stands and which
					// way it lies, for the shader to hold against what was baked.
					// (Direction k of the bake looks along (cos k*22.5, sin k*22.5)
					// in x and z.) Without one a white texel stands in and z says off.
					TerrainHorizon* horizon = (Scene != NULL && Scene->GetTerrainHorizon()) ? Scene->GetTerrainHorizon().get() : NULL;
					Texture* horizonTexture = (horizon != NULL && horizon->GetTexture() != NULL) ? horizon->GetTexture() : ssaoWhite;
					horizonTexture->Bind();
					int horizonUnit = Texture::GetLastBindedUnit();
					dirHorizonMapHandle->SetValue(&horizonUnit);
					Vec4 horizonRect, horizonSun;
					if (horizonTexture != ssaoWhite)
					{
						const Vec3 toSun = (d->GetOwner()->GetWorldTransformation() * Vec4(d->GetLightDirection(), 0.f)).xyz().normalize() * -1.f;
						f32 turn = atan2f(toSun.z, toSun.x) / 6.28318531f;
						if (turn < 0.f) turn += 1.f;
						horizonRect = horizon->GetRect();
						horizonSun = Vec4(turn * (f32)TerrainHorizon::Directions, toSun.y, 1.f, horizon->softness);
					}
					dirHorizonRectHandle->SetValue(&horizonRect);
					dirHorizonSunHandle->SetValue(&horizonSun);

					RenderObject(directionalLight->GetMeshes()[0], d->GetOwner(), deferredMaterialDirectional);
					horizonTexture->Unbind();

					if (d->IsCastingShadows())
					{
						d->GetShadowMapTexture()->Unbind();
						// One matrix per *cascade*, not one per light: the
						// shadow pass pushes inside its
						// `for i < GetNumberCascades()` loop (IRenderer.cpp),
						// and the read above is DirectionalShadowMatrix
						// [numberDir + j] for j over the cascades. Advancing
						// by 1 here meant a second shadow-casting
						// directional light started reading at cascade 1 of
						// the first light's block instead of at its own -
						// every cascade after the first light misaligned by
						// numberCascades-1. Invisible with a single
						// directional light, which is why it survived.
						numberDir += d->GetNumberCascades();
					}
					else
					{
						dummyShadow2D->Unbind();
					}
				}
				break;
				};
			}
		}

		aoTexture->Unbind();

		// Prepare and Pack Lights to Send to Shaders
		std::vector<Matrix> _Lights;

		if (lcomps.size() > 0)
		{
			uint32 pointCounter = 0;
			uint32 spotCounter = 0;
			for (std::vector<IComponent*>::iterator i = lcomps.begin(); i != lcomps.end(); i++)
			{
				switch (((ILightComponent*)(*i))->GetLightType())
				{
					case LIGHT_TYPE::DIRECTIONAL:
					{
						DirectionalLight* d = ((DirectionalLight*)(*i));

						// Directional Lights
						Vec4 color = d->GetLightRadiance();
						Vec3 position;
						Vec3 direction = (d->GetOwner()->GetWorldTransformation() * Vec4(d->GetLightDirection(), 0.f)).xyz().normalize();
						f32 attenuation = 1.f;
						Vec2 cones;
						int32 type = 1;

						Matrix directionalLight = Matrix();
						directionalLight.m[0] = color.x;         directionalLight.m[1] = color.y;             directionalLight.m[2] = color.z;             directionalLight.m[3] = color.w;
						directionalLight.m[4] = position.x;      directionalLight.m[5] = position.y;          directionalLight.m[6] = position.z;
						directionalLight.m[7] = direction.x;     directionalLight.m[8] = direction.y;         directionalLight.m[9] = direction.z;
						directionalLight.m[10] = 0.0f;			 directionalLight.m[11] = 0.0f;				  directionalLight.m[12] = 0.0f;
						directionalLight.m[13] = (f32)type;	  	 directionalLight.m[14] = d->GetShadowFilterPacked();  directionalLight.m[15] = (d->IsCastingShadows() ? 1.f : 0.f);

						_Lights.push_back(directionalLight);
						// NumberOfDirectionalShadows is set in PreRender only.
					}
					break;
					case LIGHT_TYPE::POINT:
					{
						PointLight* p = ((PointLight*)(*i));

						// Point Lights
						Vec4 color = p->GetLightRadiance();
						Vec3 position = (p->GetOwner()->GetWorldPosition());
						Vec3 direction;
						f32 attenuation = p->GetLightRadius();
						Vec2 cones;
						int32 type = 2;

						Matrix pointLight = Matrix();
						pointLight.m[0] = color.x;       pointLight.m[1] = color.y;           pointLight.m[2] = color.z;           pointLight.m[3] = color.w;
						pointLight.m[4] = position.x;    pointLight.m[5] = position.y;        pointLight.m[6] = position.z;
						pointLight.m[7] = direction.x;   pointLight.m[8] = direction.y;       pointLight.m[9] = direction.z;
						pointLight.m[10] = attenuation;  pointLight.m[11] = 0.f;				  pointLight.m[12] = 0.f;
						pointLight.m[13] = (f32)type;	 pointLight.m[14] = p->GetShadowFilterPacked();

						if (p->IsCastingShadows())
						{
							pointLight.m[14] = p->GetShadowFilterPacked();
							pointLight.m[15] = (f32)pointCounter++;
							// NumberOfPointShadows counted in PreRender only.
						}

						_Lights.push_back(pointLight);
					}
					break;
					case LIGHT_TYPE::SPOT:
					{
						SpotLight* s = ((SpotLight*)(*i));

						// Spot Lights
						Vec4 color = s->GetLightRadiance();
						Vec3 position = s->GetOwner()->GetWorldPosition();
						Vec3 direction = (s->GetOwner()->GetWorldTransformation() * Vec4(s->GetLightDirection(), 0.f)).xyz().normalize();
						f32 attenuation = s->GetLightRadius();
						Vec2 cones = Vec2(s->GetLightCosInnerCone(), s->GetLightCosOutterCone());
						int32 type = 3;

						Matrix spotLight = Matrix();
						spotLight.m[0] = color.x;        spotLight.m[1] = color.y;            spotLight.m[2] = color.z;            spotLight.m[3] = color.w;
						spotLight.m[4] = position.x;     spotLight.m[5] = position.y;         spotLight.m[6] = position.z;
						spotLight.m[7] = direction.x;    spotLight.m[8] = direction.y;        spotLight.m[9] = direction.z;
						spotLight.m[10] = attenuation;	 spotLight.m[11] = cones.x;			  spotLight.m[12] = cones.y;
						spotLight.m[13] = (f32)type;

						if (s->IsCastingShadows())
						{
							spotLight.m[14] = s->GetShadowFilterPacked();
							spotLight.m[15] = (f32)spotCounter++;
							// NumberOfSpotShadows counted in PreRender only.
						}

						_Lights.push_back(spotLight);

					};

					// Universal Cache
					ProjectionMatrix = ScenePassProjection(projection);
					NearFarPlane = Vec2(projection.Near, projection.Far);

				}
			}
		}

		// Release the G-buffer's texture units *before* the translucent pass
		// below, not after it. The light passes above are the last thing
		// that needs them, and Texture::Bind()/Unbind() share one global
		// unit counter: leaving five attachments bound meant every material
		// in the translucent pass got units 5+, while GenericShaderMaterial
		// sends its sampler uniforms as the texture's index in its own
		// Textures list (0, 1, ...) - an assumption that only holds when the
		// counter starts at zero, as it always does under ForwardRenderer.
		// So a textured transparent object sampled a G-buffer attachment
		// instead of its own texture: text rendered as solid blocks of the
		// font colour (uFontmap read attachment 0 - depth - which is 1.0
		// almost everywhere, so every glyph quad came out fully covered).
		// Unbinding here restores exactly the state ForwardRenderer gives
		// these same materials.
		for (int i = FBO->GetAttachments().size() - 1; i >= 0; i--)
			FBO->GetAttachments()[i]->TexturePTR->Unbind();

		// Scissor Test
		StartScissorTest();

		EndClippingPlanes();

		// Render Translucid Meshes
		for (std::vector<RenderingMesh*>::iterator i = rmesh.begin(); i != rmesh.end(); i++)
		{

			Lights.clear();
			if ((*i)->Material->IsTransparent() && (*i)->renderingComponent->GetOwner() != NULL)
			{
				// Culling Test
				bool cullingTest = false;
				switch ((*i)->CullingGeometry)
				{
				case CullingGeometry::Box:
					cullingTest = CullingBoxTest((*i), (*i)->renderingComponent->GetOwner());
					break;
				case CullingGeometry::Sphere:
				default:
					cullingTest = CullingSphereTest((*i), (*i)->renderingComponent->GetOwner());
					break;
				}
				if (!(*i)->renderingComponent->IsCullTesting()) cullingTest = true;
				if (cullingTest && (*i)->renderingComponent->IsActive() && (*i)->Active == true)
				{
					// The bounding sphere's centre - see CullingSphereTest.
					Vec3 objectPosition = (*i)->renderingComponent->GetOwner()->GetWorldTransformation() * (*i)->renderingComponent->GetOwner()->GetBoundingSphereCenter();
					for (std::vector<Matrix>::iterator _l = _Lights.begin(); _l != _Lights.end(); _l++)
					{
						if ((*_l).m[13] == 1) Lights.push_back(*_l);
						else if ((*_l).m[13] == 2 || (*_l).m[13] == 3)
						{
							Vec3 _lPos = Vec3((*_l).m[4], (*_l).m[5], (*_l).m[6]);
							if ((_lPos.distance(objectPosition) - ((*i)->renderingComponent->GetOwner()->GetBoundingSphereRadiusWorldSpace())) < (*_l).m[10])
								Lights.push_back(*_l);
						}
					}

					// Same reasoning as ForwardRenderer::RenderScene(): sort
					// nearest-first so that if there are more relevant lights
					// than PyrosShader.glsl's MAX_LIGHTS, it's the farthest
					// ones that get dropped by the UBO upload's clamp, not an
					// arbitrary subset in scene-registration order.
					std::stable_sort(Lights.begin(), Lights.end(), [&objectPosition](const Matrix &a, const Matrix &b) {
						bool aDirectional = (a.m[13] == 1);
						bool bDirectional = (b.m[13] == 1);
						if (aDirectional != bDirectional) return aDirectional;
						if (aDirectional) return false;
						f32 aDistSQR = Vec3(a.m[4], a.m[5], a.m[6]).distanceSQR(objectPosition);
						f32 bDistSQR = Vec3(b.m[4], b.m[5], b.m[6]).distanceSQR(objectPosition);
						return aDistSQR < bDistSQR;
					});

					NumberOfLights = Lights.size();
					// A single-target forward pass: a CustomShaderMaterial
					// compiled for the G-buffer must draw with its forward
					// variant here, or its FragData_r write lands unlit as
					// the object's final colour (and a skinned mesh needs
					// the skinned one). See UseVariantForNextDraw().
					IMaterial* mat = (*i)->Material.get();
					CustomShaderMaterial* csm = (typeid(*mat) == typeid(CustomShaderMaterial)) ? static_cast<CustomShaderMaterial*>(mat) : nullptr;
					const bool usedCustomSwap = csm && csm->UseVariantForNextDraw(false, (*i)->SkinningBones.size() > 0);
					RenderObject((*i), (*i)->renderingComponent->GetOwner(), mat);
					if (usedCustomSwap)
						csm->RestoreOwnProgram();
				}
			}
		}

		// Disable Scissor Test
		EndScissorTest();

		// End Clipping Planes
		EndClippingPlanes();

		// End Rendering
		EndRender();

		// (The G-buffer attachments were already unbound above, ahead of the
		// translucent pass - see the comment there.)

		lastPassFBO->UnBind();
		FrameProfiler::Instance().End();

		// See SetSkipRenderToScreen()'s comment: this whole block re-draws
		// the already-finished composite (colorTexture, untouched by
		// skipping this) as one more full-screen pass unconditionally
		// targeting framebuffer 0 - real output only for a DeferredRenderer
		// that IS the application's whole direct output, actively harmful
		// for any caller (every editor use today) that only ever reads
		// GetColorTexture() and may have another such caller also drawing
		// to framebuffer 0 in the same frame.
		//
		// SSR is the exception: it only exists in that pass's shader, so a
		// caller that reads GetColorTexture() (the editor's viewport) would
		// never see a reflection. With SSR on, the pass runs into
		// ssrOutTexture instead of framebuffer 0 and GetColorTexture()
		// hands that out - the same pixels the Player puts on screen.
		const bool ssrToTexture = skipRenderToScreen && ssrEnabled > 0.5f;
		ssrOutputValid = false;
		if (ssrToTexture)
		{
			if (!ssrOutTexture)
			{
				ssrOutTexture = new Texture();
				ssrOutTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA16F, this->Width, this->Height, false);
				ssrOutTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
				ssrOutFBO = new FrameBuffer();
				ssrOutFBO->SetDebugName("Deferred SSR output");
				ssrOutFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, ssrOutTexture);
			}
			ssrOutFBO->Bind();
		}
		if (!skipRenderToScreen || ssrToTexture) {
			ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
			ClearDepthBuffer();
			ClearScreen();

			InitRender();

			// deferredLastPass's material-aware SSR needs tDepth/tNormal/
			// tMetallicRoughness/tDiffuse again - already unbound above
			// (restoring texture-unit bookkeeping to a clean state), so
			// re-bound here in a fresh, self-contained sequence just for this
			// draw rather than reordering the unbind above (which other code
			// relies on running where it already does). Units 0-5 in binding
			// order below match deferredLastPass's tDepth/tNormal/
			// tMetallicRoughness/tColor/tPreviousFrameColor/tDiffuse uniform
			// values set in the constructor.
			GetGBufferAttachment(FrameBufferAttachmentFormat::Depth_Attachment)->Bind();
			GetGBufferAttachment(FrameBufferAttachmentFormat::Color_Attachment2)->Bind();
			GetGBufferAttachment(FrameBufferAttachmentFormat::Color_Attachment3)->Bind();
			colorTexture->Bind();
			previousFrameColorTexture->Bind();
			GetGBufferAttachment(FrameBufferAttachmentFormat::Color_Attachment0)->Bind();
			// Real mip count of previousFrameColorTexture at the current
			// size - see lastPass.glsl's uMaxReflectionLod/textureLod()
			// comment. Recomputed every frame rather than cached/only on
			// resize - log2() on two ints is free next to everything else
			// in this function, and this keeps it correct without having to
			// remember to also update it in Resize().
			f32 maxReflectionLod = log2f((f32)(Width > Height ? Width : Height));
			lastPassMaxReflectionLodHandle->SetValue(&maxReflectionLod);
			// Feed this draw with what's still *last* frame's real camera -
			// ssrPrvViewMatrix/ssrPrvProjectionMatrix aren't updated to this
			// frame's Camera/projection until after the draw below (see that
			// update's comment). See DeferredRenderer.h's comment on these
			// members for why this is dedicated state instead of IRenderer's
			// shared Prv* (which PreRender() clobbers before this point).
			lastPassPrvViewMatrixHandle->SetValue(&ssrPrvViewMatrix);
			lastPassPrvProjectionMatrixHandle->SetValue(&ssrPrvProjectionMatrix);
			// Re-poke SSR uniforms every frame so EnableSSR()/SetSSRDistances()
			// values are definitely in the UBO scratch (Other uniforms are only
			// copied from Uniform::Value during CaptureExtraUniform).
			lastPassSSREnabledHandle->SetValue(&ssrEnabled);
			lastPassSSRStepDistanceHandle->SetValue(&ssrStepDistance);
			lastPassSSRMaxDistanceHandle->SetValue(&ssrMaxDistance);
			lastPassSSRDebugHandle->SetValue(&ssrDebugMode);
			// Render to Screen
			{
				GameObject go = GameObject();
				RenderObject(directionalLight->GetMeshes()[0], &go, deferredLastPass);
			}
			GetGBufferAttachment(FrameBufferAttachmentFormat::Color_Attachment0)->Unbind();
			previousFrameColorTexture->Unbind();
			colorTexture->Unbind();
			GetGBufferAttachment(FrameBufferAttachmentFormat::Color_Attachment3)->Unbind();
			GetGBufferAttachment(FrameBufferAttachmentFormat::Color_Attachment2)->Unbind();
			GetGBufferAttachment(FrameBufferAttachmentFormat::Depth_Attachment)->Unbind();
			if (ssrToTexture)
			{
				ssrOutFBO->UnBind();
				ssrOutputValid = true;
			}
		}
		// Next frame's SSR reprojection needs this frame's real camera
		// regardless of whether the screen draw above ran - kept outside
		// the block above (was previously sandwiched inside it, right after
		// the draw it doesn't actually depend on).
		ssrPrvViewMatrix = Camera->GetWorldTransformation().Inverse();
		ssrPrvProjectionMatrix = projection.m;

		// No "restore the caller's FBO" replay needed here (unlike this
		// block's old position before the lastPass draw, which genuinely
		// needed one): FrameBuffer::UnBind() always issues an
		// unconditional rebind-to-0 as its first action regardless of
		// whatever the device's draw target actually is, so the caller's
		// later EndCapture() (ExternalFBO->UnBind()) doesn't depend on
		// it. A replay *was* tried here and was actively harmful on
		// Vulkan: rebinding an FBO with an already-built render pass
		// (ExternalFBO, having just been drawn into by deferredLastPass
		// above) really begins a fresh render pass for it - LOAD_OP_CLEAR
		// wiped the frame that was just drawn, back to black, the moment
		// this ran after the draw instead of before it. Found via a real
		// user report (DeferredPBRSpheres - no SSR-specific code at all -
		// going black too, confirming this affected every DeferredRenderer
		// caller, not something SSRTest-specific).

		EndRender();

		if (ownFrame)
			device->EndFrame();
	}

	Texture* DeferredRenderer::GetGBufferAttachment(const uint32 attachmentFormat) const
	{
		const std::vector<FBOAttachment*> &attachments = FBO->GetAttachments();
		for (size_t i = 0; i < attachments.size(); i++)
			if (attachments[i]->EngineAttachmentFormat == attachmentFormat)
				return attachments[i]->TexturePTR;
		return NULL;
	}

	void DeferredRenderer::SetFBO(FrameBuffer* fbo)
	{
		// Save FBO
		FBO = fbo;
	}

	void DeferredRenderer::SetSSRDistances(const f32 stepDistance, const f32 maxDistance)
	{
		ssrStepDistance = stepDistance;
		ssrMaxDistance = maxDistance;
		lastPassSSRStepDistanceHandle->SetValue(&ssrStepDistance);
		lastPassSSRMaxDistanceHandle->SetValue(&ssrMaxDistance);
	}

	void DeferredRenderer::EnableSSR()
	{
		ssrEnabled = 1.0f;
		lastPassSSREnabledHandle->SetValue(&ssrEnabled);
	}

	void DeferredRenderer::DisableSSR()
	{
		ssrEnabled = 0.0f;
		lastPassSSREnabledHandle->SetValue(&ssrEnabled);
	}

	void DeferredRenderer::SetSSRDebugMode(const uint32 mode)
	{
		ssrDebugMode = (f32)mode;
		lastPassSSRDebugHandle->SetValue(&ssrDebugMode);
	}

	void DeferredRenderer::SetupSSAOMaterial(CustomShaderMaterial *material, Uniform *handles[5])
	{
		material->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		material->AddUniform(Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));
		material->AddUniform(Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));
		handles[0] = material->AddUniform(Uniform("uSSAORadius", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		handles[1] = material->AddUniform(Uniform("uSSAOStrength", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		handles[2] = material->AddUniform(Uniform("uSSAOFalloff", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		handles[3] = material->AddUniform(Uniform("uSSAOSamples", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		handles[4] = material->AddUniform(Uniform("uSSAODirect", Uniforms::DataUsage::Other, Uniforms::DataType::Float));

		// DeferredSSAOParams in deferredSSAO.glsl / deferredSSAOBlur.glsl,
		// std140 by hand: two vec2s, a mat4 at 16, then five floats, and
		// the block rounded up to a multiple of 16.
		IMaterial::ExtraUniformsBlock &block = material->extraUniforms[0];
		block.binding = 35;
		block.blockName = "DeferredSSAOParams";
		block.size = 112;
		block.scratch.resize(block.size, 0);
		block.offsets["uScreenDimensions"] = 0;
		block.offsets["uNearFar"] = 8;
		block.offsets["uMatProj"] = 16;
		block.offsets["uSSAORadius"] = 80;
		block.offsets["uSSAOStrength"] = 84;
		block.offsets["uSSAOFalloff"] = 88;
		block.offsets["uSSAOSamples"] = 92;
		block.offsets["uSSAODirect"] = 96;
		// See deferredLastPass's identical reset: the auto-populated
		// fragment block would upload a second buffer to the same binding.
		material->extraUniforms[1].binding = 0;
		material->extraUniforms[1].bufferHandle = 0;
		material->extraUniforms[1].size = 0;
		material->extraUniforms[1].offsets.clear();
		material->extraUniforms[1].scratch.clear();

		material->DisableDepthTest();
		material->DisableDepthWrite();
		// A full-screen quad - see deferredMaterialAmbient's comment.
		material->SetCullFace(CullFace::DoubleSided);
	}

	void DeferredRenderer::SetSSAODirectStrength(const f32 direct)
	{
		ssaoDirect = direct < 0.f ? 0.f : (direct > 1.f ? 1.f : direct);
	}

	void DeferredRenderer::EnableSSAO()
	{
		ssaoEnabled = true;
	}

	void DeferredRenderer::DisableSSAO()
	{
		ssaoEnabled = false;
	}

	void DeferredRenderer::SetSSAOParams(const f32 radius, const f32 strength, const f32 falloff)
	{
		ssaoRadius = radius;
		ssaoStrength = strength;
		ssaoFalloff = falloff;
	}

};
