//============================================================================
// Name        : PointLight.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Point Light
//============================================================================

#include <Pyros3D/Rendering/Components/Lights/PointLight/PointLight.h>

namespace p3d {

	PointLight::PointLight(const Vec4 &color, const f32 radius) : ILightComponent(LIGHT_TYPE::POINT)
	{
		Color = color;
		Radius = radius;
		ShadowBiasScale = 0.02f;

		// Bounding
		minBounds = Vec3(-radius*.5f, -radius*.5f, -radius*.5f);
		maxBounds = Vec3(radius*.5f, radius*.5f, radius*.5f);
		BoundingSphereCenter = Vec3();
		BoundingSphereRadius = radius;
	}

	void PointLight::EnableCastShadows(const uint32 Width, const uint32 Height, const f32 Near)
	{
		// Unlike DirectionalLight this was never guarded, so it already
		// reassigned its dimensions on a second call - but it left the
		// previous FBO and shadow texture to be overwritten without
		// waiting for the GPU that may still be reading them, which is
		// only safe on GL. Same release path as the other two now.
		if (isCastingShadows) ReleaseShadowResources();


		ShadowWidth = Width;
		ShadowHeight = Height;

		// Set Flag
		isCastingShadows = true;

		// Initiate FBO (releases any previously owned FBO/texture first)
		shadowsFBO.reset(new FrameBuffer());

		ShadowMap.reset(new Texture());

#if defined(GLLEGACY)
		// Regular Shadows
		// Create Texture (CubeMap), Frame Buffer and Set the Texture as Attachment
		ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_X, TextureDataType::RGBA, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_Y, TextureDataType::RGBA, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_Z, TextureDataType::RGBA, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_X, TextureDataType::RGBA, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_Y, TextureDataType::RGBA, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_Z, TextureDataType::RGBA, ShadowWidth, ShadowHeight, false);
		ShadowMap->SetRepeat(TextureRepeat::ClampToBorder, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		ShadowMap->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);

		// Initialize Frame Buffer
		shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, ShadowWidth, ShadowHeight);
		shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::CubemapPositive_X, ShadowMap.get());

#else
		// GPU Shadows
		// Create Texture (CubeMap), Frame Buffer and Set the Texture as Attachment
		// An R32F *colour* cubemap, not a depth cubemap. Sampling a depth
		// cube map in a shader is broken on MoltenVK: as a comparison
		// sampler it returns 1.0 unconditionally, and as a plain sampler it
		// returns ~0 for a large fraction of directions (measured: 43% of
		// floor pixels, against 0.5% on GL, with the cube map's contents
		// and the lookup direction proven identical between the two).
		// Nothing in this engine was wrong - the image held the right
		// values, the readback proved it, and the shader still could not
		// read them back.
		//
		// Storing the same value in a colour attachment sidesteps that
		// path completely, and is the usual way point-light shadows are
		// done anyway. The stored value is unchanged - PyrosShader.glsl's
		// CASTSHADOWS block writes gl_FragCoord.z, exactly what the depth
		// attachment used to receive - so PCFPOINT's reference depth and
		// its comparison need no adjustment at all.
		ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_X, TextureDataType::R32F, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_Y, TextureDataType::R32F, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_Z, TextureDataType::R32F, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_X, TextureDataType::R32F, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_Y, TextureDataType::R32F, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_Z, TextureDataType::R32F, ShadowWidth, ShadowHeight, false);
		ShadowMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		ShadowMap->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);

		// Colour attachment now, plus a real depth buffer: the faces are
		// still rasterized with depth testing (nearest occluder wins), it
		// is only the *stored* result that moved from the depth attachment
		// to the colour one.
		shadowsFBO->Init(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::CubemapPositive_X, ShadowMap.get());
		shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, ShadowWidth, ShadowHeight);

#endif

		// Near and Far Clip Planes
		ShadowNear = Near;
	}

}