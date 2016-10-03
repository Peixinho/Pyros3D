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

		// Bounding
		minBounds = Vec3(-radius*.5f, -radius*.5f, -radius*.5f);
		maxBounds = Vec3(radius*.5f, radius*.5f, radius*.5f);
		BoundingSphereCenter = Vec3();
		BoundingSphereRadius = radius;
	}

	void PointLight::EnableCastShadows(const uint32 Width, const uint32 Height, const f32 Near)
	{

		ShadowWidth = Width;
		ShadowHeight = Height;

		// Set Flag
		isCastingShadows = true;

		// Initiate FBO
		shadowsFBO = new FrameBuffer();

		ShadowMap = new Texture();

#if defined(GLES2) || defined(GL_LEGACY)
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
		shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::CubemapPositive_X, ShadowMap);

#else
		// GPU Shadows
		// Create Texture (CubeMap), Frame Buffer and Set the Texture as Attachment
		ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_X, TextureDataType::DepthComponent, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_Y, TextureDataType::DepthComponent, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapNegative_Z, TextureDataType::DepthComponent, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_X, TextureDataType::DepthComponent, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_Y, TextureDataType::DepthComponent, ShadowWidth, ShadowHeight, false);
		ShadowMap->CreateEmptyTexture(TextureType::CubemapPositive_Z, TextureDataType::DepthComponent, ShadowWidth, ShadowHeight, false);
		ShadowMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		ShadowMap->EnableCompareMode();

		// Initialize Frame Buffer
		shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::CubemapPositive_X, ShadowMap);

#endif

		// Near and Far Clip Planes
		ShadowNear = Near;
	}

}