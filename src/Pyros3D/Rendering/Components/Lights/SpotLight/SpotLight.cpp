//============================================================================
// Name        : SpotLight.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Spot Light
//============================================================================

#include <Pyros3D/Rendering/Components/Lights/SpotLight/SpotLight.h>

namespace p3d {

	SpotLight::SpotLight(const Vec4 &color, const f32 radius, const Vec3 &direction, const f32 OutterCone, const f32 InnerCone) : ILightComponent(LIGHT_TYPE::SPOT)
	{
		Color = color;
		Radius = radius;
		innerCone = InnerCone;
		outterCone = OutterCone;
		CosOutterCone = cosf((f32)DEGTORAD(OutterCone));
		CosInnerCone = cosf((f32)DEGTORAD(InnerCone));
		Direction = direction;

		// Bounding
		minBounds = Vec3(-radius*.5f, -radius*.5f, -radius*.5f);
		maxBounds = Vec3(radius*.5f, radius*.5f, radius*.5f);
		BoundingSphereCenter = Vec3();
		BoundingSphereRadius = radius;
	}

	void SpotLight::EnableCastShadows(const uint32 Width, const uint32 Height, const f32 Near)
	{
		ShadowWidth = Width;
		ShadowHeight = Height;

		// Set Flag
		isCastingShadows = true;

		// Initiate FBO
		shadowsFBO = new FrameBuffer();

		// GPU Shadows
		ShadowMap = new Texture();

#if defined(GLES2) || defined(GL_LEGACY)

		ShadowMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, ShadowWidth, ShadowHeight, false);
		ShadowMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		// Initialize Frame Buffer
		shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, ShadowWidth, ShadowHeight);
		shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, ShadowMap);

#else

		ShadowMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, ShadowWidth, ShadowHeight, false);
		ShadowMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		ShadowMap->EnableCompareMode();

		// Initialize Frame Buffer
		shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, ShadowMap);

#endif

		// Near and Far Clip Planes
		ShadowNear = Near;
		ShadowFar = Radius;
	}

}