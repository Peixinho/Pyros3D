//============================================================================
// Name        : DeferredRenderer.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Deferred Renderer
//============================================================================

#include <Pyros3D/Rendering/Renderer/DeferredRenderer/DeferredRenderer.h>
#include <Pyros3D/Other/PyrosGL.h>

namespace p3d {

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

		echo("SUCCESS: Deferred Renderer Created");

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

		// See DeferredRenderer.h's comment on forwardDepthTexture - a real
		// copy of fbo's depth attachment, refreshed every frame in
		// RenderScene(), used as lastPassFBO's depth attachment instead
		// of directly aliasing fbo's own depth texture.
		forwardDepthTexture = new Texture();
		forwardDepthTexture->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, Width, Height, false);
		forwardDepthTexture->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		lastPassFBO = new FrameBuffer();
		lastPassFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, colorTexture);
		lastPassFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, forwardDepthTexture);

		// See DeferredRenderer.h's comment on dummyShadow2D/dummyShadowCube.
		// Same creation pattern DirectionalLight/PointLight's own real
		// EnableCastShadows() uses, just tiny (contents never sampled).
		dummyShadow2D = new Texture();
		dummyShadow2D->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, 4, 4, false);
		dummyShadow2D->SetRepeat(TextureRepeat::Clamp, TextureRepeat::Clamp);
		dummyShadow2D->EnableCompareMode();

		dummyShadowCube = new Texture();
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapNegative_X, TextureDataType::DepthComponent, 4, 4, false);
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapNegative_Y, TextureDataType::DepthComponent, 4, 4, false);
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapNegative_Z, TextureDataType::DepthComponent, 4, 4, false);
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapPositive_X, TextureDataType::DepthComponent, 4, 4, false);
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapPositive_Y, TextureDataType::DepthComponent, 4, 4, false);
		dummyShadowCube->CreateEmptyTexture(TextureType::CubemapPositive_Z, TextureDataType::DepthComponent, 4, 4, false);
		dummyShadowCube->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		dummyShadowCube->EnableCompareMode();
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

		uint32 colorID = 0;
		deferredLastPass->AddUniform(Uniform("tColor", Uniforms::DataType::Int, &colorID));
		deferredLastPass->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));

		// See IMaterial.h's comment on extraUniforms[2] - matches the
		// LastPassFragParams block declared in shaders/lastPass.glsl exactly.
		deferredLastPass->extraUniforms[0].binding = 37;
		deferredLastPass->extraUniforms[0].blockName = "LastPassFragParams";
		deferredLastPass->extraUniforms[0].size = 8;
		deferredLastPass->extraUniforms[0].scratch.resize(deferredLastPass->extraUniforms[0].size, 0);
		deferredLastPass->extraUniforms[0].offsets["uScreenDimensions"] = 0;
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
		deferredMaterialAmbient->BlendingFunction(BlendFunc::One, BlendFunc::One);
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

		deferredMaterialDirectional->AddUniform(Uniform("uScreenDimensions", Uniforms::DataUsage::ScreenDimensions));
		dirDirHandle = deferredMaterialDirectional->AddUniform(Uniform("uLightDirection", Uniforms::DataUsage::Other, Uniforms::DataType::Vec3));
		dirColorHandle = deferredMaterialDirectional->AddUniform(Uniform("uLightColor", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		dirShadowHandle = deferredMaterialDirectional->AddUniform(Uniform("uShadowMap", Uniforms::DataUsage::Other, Uniforms::DataType::Int));
		dirShadowPCFTexelHandle = deferredMaterialDirectional->AddUniform(Uniform("uPCFTexelSize", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		dirShadowDepthsMVPHandle = deferredMaterialDirectional->AddUniform(Uniform("uDirectionalDepthsMVP", Uniforms::DataUsage::Other, Uniforms::DataType::Matrix));
		dirShadowFarHandle = deferredMaterialDirectional->AddUniform(Uniform("uDirectionalShadowFar", Uniforms::DataUsage::Other, Uniforms::DataType::Vec4));
		dirHaveShadowHandle = deferredMaterialDirectional->AddUniform(Uniform("uHaveShadowmap", Uniforms::DataUsage::Other, Uniforms::DataType::Float));
		deferredMaterialDirectional->AddUniform(Uniform("uMatProj", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialDirectional->AddUniform(Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));

		// See IMaterial.h's comment on extraUniforms[2] - matches the
		// DirectionalFragParams block declared in
		// shaders/secondpassDirectional.glsl exactly, std140 offsets
		// computed by hand (vec2/vec3/vec4/vec2 pack per their own
		// alignment, each mat4 rounds up to the next 16-byte boundary).
		deferredMaterialDirectional->extraUniforms[0].binding = 32;
		deferredMaterialDirectional->extraUniforms[0].blockName = "DirectionalFragParams";
		deferredMaterialDirectional->extraUniforms[0].size = 420;
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
		deferredMaterialPoint->AddUniform(Uniform("uModelMatrix", Uniforms::DataUsage::ModelMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uViewMatrix", Uniforms::DataUsage::ViewMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uProjectionMatrix", Uniforms::DataUsage::ProjectionMatrix));
		deferredMaterialPoint->AddUniform(Uniform("uNearFar", Uniforms::DataUsage::NearFarPlane));

		// See IMaterial.h's comment on extraUniforms[2] - two separate
		// blocks matching secondpassPoint.glsl's PointVertParams (VERTEX
		// stage, binding 33) and PointFragParams (FRAGMENT stage, binding
		// 38) exactly - see that file's comment on why it's two blocks,
		// not one combined block used by both stages.
		deferredMaterialPoint->extraUniforms[0].binding = 33;
		deferredMaterialPoint->extraUniforms[0].blockName = "PointVertParams";
		deferredMaterialPoint->extraUniforms[0].size = 192;
		deferredMaterialPoint->extraUniforms[0].scratch.resize(deferredMaterialPoint->extraUniforms[0].size, 0);
		deferredMaterialPoint->extraUniforms[0].offsets["uProjectionMatrix"] = 0;
		deferredMaterialPoint->extraUniforms[0].offsets["uViewMatrix"] = 64;
		deferredMaterialPoint->extraUniforms[0].offsets["uModelMatrix"] = 128;

		deferredMaterialPoint->extraUniforms[1].binding = 38;
		deferredMaterialPoint->extraUniforms[1].blockName = "PointFragParams";
		deferredMaterialPoint->extraUniforms[1].size = 200;
		deferredMaterialPoint->extraUniforms[1].scratch.resize(deferredMaterialPoint->extraUniforms[1].size, 0);
		deferredMaterialPoint->extraUniforms[1].offsets["uScreenDimensions"] = 0;
		deferredMaterialPoint->extraUniforms[1].offsets["uLightPosition"] = 16;
		deferredMaterialPoint->extraUniforms[1].offsets["uLightRadius"] = 28;
		deferredMaterialPoint->extraUniforms[1].offsets["uLightColor"] = 32;
		deferredMaterialPoint->extraUniforms[1].offsets["uNearFar"] = 48;
		deferredMaterialPoint->extraUniforms[1].offsets["uPointDepthsMVP"] = 64;
		deferredMaterialPoint->extraUniforms[1].offsets["uPCFTexelSize"] = 192;
		deferredMaterialPoint->extraUniforms[1].offsets["uHaveShadowmap"] = 196;

		// Real, pre-existing bug (predates this session's HDR work - traced
		// back to a screenshot taken right after the black-screen/culling
		// fix, before any tonemap code existed) found chasing a report
		// that Vulkan's point/spot lights contributed no visible
		// shading/highlights at all - just a flat per-object ambient tint,
		// on both DeferredPBRSpheres AND (silently, unnoticed until now)
		// the original black-screen investigation's own verification
		// screenshot. This light-volume geometry is a Sphere primitive
		// (`sphereHandle` below), whose hand-authored index winding is
		// documented elsewhere in this codebase (examples/PBRSpheres.cpp's
		// identical comment) as opposite Cube's - CullFace::FrontFace
		// (culling the *near* faces, the standard "camera may be inside
		// the light volume" deferred-shading technique) was tuned against
		// that reversed winding and happened to produce visible output on
		// GL, but on Vulkan culled away all or nearly all of the volume's
		// fragments, leaving only the separately-rendered ambient pass
		// visible. CullFace::BackFace (this material's default anyway,
		// kept explicit for clarity) is correct and verified identical on
		// both backends - real gradient shading and specular highlights
		// confirmed via screenshot on GL and Vulkan alike.
		deferredMaterialPoint->SetCullFace(CullFace::BackFace);
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

		// See IMaterial.h's comment on extraUniforms[2] - two separate
		// blocks matching secondpassSpot.glsl's SpotVertParams (VERTEX
		// stage, binding 34) and SpotFragParams (FRAGMENT stage, binding
		// 39) exactly (see secondpassPoint.glsl's comment on why it's two
		// blocks, not one combined block used by both stages).
		deferredMaterialSpot->extraUniforms[0].binding = 34;
		deferredMaterialSpot->extraUniforms[0].blockName = "SpotVertParams";
		deferredMaterialSpot->extraUniforms[0].size = 192;
		deferredMaterialSpot->extraUniforms[0].scratch.resize(deferredMaterialSpot->extraUniforms[0].size, 0);
		deferredMaterialSpot->extraUniforms[0].offsets["uProjectionMatrix"] = 0;
		deferredMaterialSpot->extraUniforms[0].offsets["uViewMatrix"] = 64;
		deferredMaterialSpot->extraUniforms[0].offsets["uModelMatrix"] = 128;

		deferredMaterialSpot->extraUniforms[1].binding = 39;
		deferredMaterialSpot->extraUniforms[1].blockName = "SpotFragParams";
		deferredMaterialSpot->extraUniforms[1].size = 232;
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

		// See deferredMaterialPoint's identical comment above - same
		// Sphere-primitive light volume, same fix.
		deferredMaterialSpot->SetCullFace(CullFace::BackFace);
		deferredMaterialSpot->DisableDepthTest();
		deferredMaterialSpot->DisableDepthWrite();
		deferredMaterialSpot->EnableBlending();
		deferredMaterialSpot->BlendingEquation(BlendEq::Add);
		deferredMaterialSpot->BlendingFunction(BlendFunc::One, BlendFunc::One);

		// Light Volume
		quadHandle = new Plane(1, 1);
		directionalLight = new RenderingComponent(quadHandle);

		sphereHandle = new Sphere(1, 6, 4);
		pointLight = new RenderingComponent(sphereHandle);
		// This mesh's own default Material is never actually used for
		// drawing (every real draw call passes deferredMaterialPoint/Spot
		// explicitly as an override - see the light-rendering loop below)
		// but kept consistent with them regardless, matching their
		// identical CullFace fix/comment above.
		pointLight->GetMeshes()[0]->Material->SetCullFace(CullFace::BackFace);
	}

	void DeferredRenderer::Resize(const uint32 Width, const uint32 Height)
	{
		IRenderer::Resize(Width, Height);
		lastPassFBO->Resize(Width, Height);
	}

	DeferredRenderer::~DeferredRenderer()
	{
		delete lastPassFBO;
		delete colorTexture;
		delete forwardDepthTexture;
		delete dummyShadow2D;
		delete dummyShadowCube;
		delete shadowMaterial;
		delete shadowSkinnedMaterial;
		delete sphereHandle;
		delete quadHandle;
		delete deferredLastPass;
		delete deferredMaterialAmbient;
		delete deferredMaterialDirectional;
		delete deferredMaterialPoint;
		delete deferredMaterialSpot;
		delete directionalLight;
		delete pointLight;
	}

	void DeferredRenderer::RenderScene(const p3d::Projection& projection, GameObject* Camera, SceneGraph* Scene, const uint32 BufferOptions)
	{

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
				warmupCubeFace.Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::CubemapPositive_X + face, dummyShadowCube);
				warmupCubeFace.Bind();
				warmupCubeFace.UnBind();
			}

			dummyShadowsWarmedUp = true;
		}

		// Initialize Renderer
		InitRender();

		// Get Lights List
		std::vector<IComponent*> lcomps = ILightComponent::GetLightsOnScene(Scene);

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

		// Universal Cache
		ProjectionMatrix = projection.m;
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
		bool isMainSwapchainPass = device->GetCurrentRenderTarget() == 0;
		if (isMainSwapchainPass)
			device->BeginFrame();

		// Bind Frame Buffer
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
		for (std::vector<RenderingMesh*>::iterator j = rmesh.begin(); j != rmesh.end(); j++)
		{

			if (!(*j)->Material->IsTransparent())
			{
				if ((*j)->renderingComponent->GetOwner() != NULL)
				{
					// Culling Test
					bool cullingTest = false;
					switch ((*j)->CullingGeometry)
					{
					case CullingGeometry::Box:
						cullingTest = CullingBoxTest((*j), (*j)->renderingComponent->GetOwner());
						break;
					case CullingGeometry::Sphere:
					default:
						cullingTest = CullingSphereTest((*j), (*j)->renderingComponent->GetOwner());
						break;
					}

					if (cullingTest && (*j)->renderingComponent->IsActive() && (*j)->Active == true)
						RenderObject((*j), (*j)->renderingComponent->GetOwner(), (*j)->Material);

				}
				else {
					break;
				}
			}
		}

		// End Rendering
		EndRender();

		// Unbind FrameBuffer
		FBO->UnBind();

		// Refresh forwardDepthTexture with this frame's real G-buffer
		// depth - see DeferredRenderer.h's comment on forwardDepthTexture
		// for why lastPassFBO can't just alias FBO's depth attachment
		// directly. Must happen after FBO->UnBind() (source depth values
		// are only final once the G-buffer pass has finished writing them)
		// and before lastPassFBO->Bind() (whose lighting materials sample
		// tDepth from FBO's original depth texture, unaffected by this
		// copy, while its own attachment set gets forwardDepthTexture).
		device->CopyDepthTexture(FBO->GetAttachments()[0]->TexturePTR->GetBindID(), forwardDepthTexture->GetBindID(), Width, Height);

		lastPassFBO->Bind();
		ClearBufferBit(Buffer_Bit::Color);
		ClearScreen();

		// Initialize Rendering
		InitRender();

		// Bind FBO Textures
		for (int i = 0;i<(int)FBO->GetAttachments().size();i++)
			FBO->GetAttachments()[i]->TexturePTR->Bind();

		// Ambient
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
					// Point Lights
					Vec3 pos = (ViewMatrix * Vec4(p->GetOwner()->GetWorldPosition(), 1.f)).xyz();
					pointPosHandle->SetValue(&pos);
					pointRadiusHandle->SetValue((void*)&p->GetLightRadius());
					pointColorHandle->SetValue((void*)&p->GetLightColor());
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
						f32 txl = p->GetShadowPCFTexelSize();
						Matrix mvp[2];
						mvp[0] = PointShadowMatrix[numberPoint];
						mvp[1] = PointShadowMatrix[numberPoint+1];
						pointShadowPCFTexelHandle->SetValue((void*)&txl);
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

					// Set Scale
					f32 sc = g(f(p->GetLightRadius()));
					Matrix m; m.Scale(sc, sc, sc);
					pointLight->GetMeshes()[0]->Pivot = m;
					RenderObject(pointLight->GetMeshes()[0], p->GetOwner(), deferredMaterialPoint);
					if (p->IsCastingShadows())
					{
						p->GetShadowMapTexture()->Unbind();
						numberPoint++;
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
					// Spot Lights
					Vec3 pos = (ViewMatrix * Vec4(s->GetOwner()->GetWorldPosition(), 1.f)).xyz();
					Vec3 dir = (ViewMatrix * (s->GetOwner()->GetWorldTransformation() * Vec4(s->GetLightDirection(), 0.f))).xyz();
					spotPosHandle->SetValue(&pos);
					spotDirHandle->SetValue(&dir);
					spotRadiusHandle->SetValue((void*)&s->GetLightRadius());
					spotOutterHandle->SetValue((void*)&s->GetLightCosOutterCone());
					spotInnerHandle->SetValue((void*)&s->GetLightCosInnerCone());
					spotColorHandle->SetValue((void*)&s->GetLightColor());

					// See the identical fix in the POINT case above.
					int shadowUnit = 4;
					float haveShadow = 0.f;
					if (s->IsCastingShadows())
					{
						f32 txl = s->GetShadowPCFTexelSize();
						Matrix mvp = SpotShadowMatrix[numberDir];
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

					// Set Scale
					f32 sc = g(f(s->GetLightRadius()));
					Matrix m; m.Scale(sc, sc, sc);
					pointLight->GetMeshes()[0]->Pivot = m;
					RenderObject(pointLight->GetMeshes()[0], s->GetOwner(), deferredMaterialSpot);

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
					dirColorHandle->SetValue((void*)&d->GetLightColor());
					// See the identical fix in the POINT case above.
					int shadowUnit = 4;
					float haveShadow = 0.f;
					if (d->IsCastingShadows())
					{
						f32 txl = d->GetShadowPCFTexelSize();

						Vec4 _ShadowFar;
						if (d->GetNumberCascades() > 0) _ShadowFar.x = d->GetCascade(0).Far;
						if (d->GetNumberCascades() > 1) _ShadowFar.y = d->GetCascade(1).Far;
						if (d->GetNumberCascades() > 2) _ShadowFar.z = d->GetCascade(2).Far;
						if (d->GetNumberCascades() > 3) _ShadowFar.w = d->GetCascade(3).Far;

						Vec4 ShadowFar;
						ShadowFar.x = 0.5f*(-_ShadowFar.x*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.x + 0.5f;
						ShadowFar.y = 0.5f*(-_ShadowFar.y*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.y + 0.5f;
						ShadowFar.z = 0.5f*(-_ShadowFar.z*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.z + 0.5f;
						ShadowFar.w = 0.5f*(-_ShadowFar.w*projection.m.m[10] + projection.m.m[14]) / _ShadowFar.w + 0.5f;

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

					RenderObject(directionalLight->GetMeshes()[0], d->GetOwner(), deferredMaterialDirectional);

					if (d->IsCastingShadows())
					{
						d->GetShadowMapTexture()->Unbind();
						numberDir++;
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
						Vec4 color = d->GetLightColor();
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
						directionalLight.m[13] = (f32)type;	  	 directionalLight.m[14] = d->GetShadowPCFTexelSize();  directionalLight.m[15] = (d->IsCastingShadows() ? 1.f : 0.f);

						_Lights.push_back(directionalLight);

						if (d->IsCastingShadows())
						{
							// Increase Number of Shadows
							NumberOfDirectionalShadows++;
						}
					}
					break;
					case LIGHT_TYPE::POINT:
					{
						PointLight* p = ((PointLight*)(*i));

						// Point Lights
						Vec4 color = p->GetLightColor();
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
						pointLight.m[13] = (f32)type;	 pointLight.m[14] = p->GetShadowPCFTexelSize();

						if (p->IsCastingShadows())
						{
							pointLight.m[14] = p->GetShadowPCFTexelSize();
							pointLight.m[15] = (f32)pointCounter++;
							NumberOfPointShadows++;
						}

						_Lights.push_back(pointLight);
					}
					break;
					case LIGHT_TYPE::SPOT:
					{
						SpotLight* s = ((SpotLight*)(*i));

						// Spot Lights
						Vec4 color = s->GetLightColor();
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
							spotLight.m[14] = s->GetShadowPCFTexelSize();
							spotLight.m[15] = (f32)spotCounter++;
							NumberOfSpotShadows++;
						}

						_Lights.push_back(spotLight);

					};

					// Universal Cache
					ProjectionMatrix = projection.m;
					NearFarPlane = Vec2(projection.Near, projection.Far);

				}
			}
		}

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
					Vec3 objectPosition = (*i)->renderingComponent->GetOwner()->GetWorldPosition();
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
					RenderObject((*i), (*i)->renderingComponent->GetOwner(), (*i)->Material);
				}
			}
		}

		// Disable Scissor Test
		EndScissorTest();

		// End Clipping Planes
		EndClippingPlanes();

		// End Rendering
		EndRender();

		for (int i = FBO->GetAttachments().size()-1; i>=0; i--)
			FBO->GetAttachments()[i]->TexturePTR->Unbind();

		lastPassFBO->UnBind();

		ClearBufferBit(Buffer_Bit::Color | Buffer_Bit::Depth);
		ClearDepthBuffer();
		ClearScreen();

		InitRender();

		colorTexture->Bind();
		// Render to Screen
		{
			GameObject go = GameObject();
			RenderObject(directionalLight->GetMeshes()[0], &go, deferredLastPass);
		}
		colorTexture->Unbind();

		EndRender();

		if (isMainSwapchainPass)
			device->EndFrame();
	}

	void DeferredRenderer::SetFBO(FrameBuffer* fbo)
	{
		// Save FBO
		FBO = fbo;
	}

};
