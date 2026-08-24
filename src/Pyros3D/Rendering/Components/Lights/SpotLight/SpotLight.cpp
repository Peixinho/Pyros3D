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
		// Far more polygon offset than a directional light's 2/1, and
		// measured rather than guessed. A spot's shadow map is a
		// *perspective* projection, so a constant offset in depth units
		// buys almost no world-space slack away from the near plane - where
		// a directional light's orthographic map gives the same slack
		// everywhere. On the Shadow Matrix scene, a shadow-casting floor
		// under a spot at 2/1 self-shadowed and blacked out the light's
		// entire lit cone on Vulkan and Metal while looking perfect on GL;
		// 12/16 cleared the floor but still self-shadowed a box top; 40/64
		// brought all three backends to within 0.05% of each other. A scene
		// that wants less can still say so - SceneSerializer only calls
		// SetShadowBias() when the keys are present.
		SetShadowBias(40.f, 64.f);
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

		// GPU Shadows
		ShadowMap.reset(new Texture());

#if defined(GLLEGACY)

		ShadowMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::RGBA, ShadowWidth, ShadowHeight, false);
		ShadowMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);

		// Initialize Frame Buffer
		shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, RenderBufferDataType::Depth, ShadowWidth, ShadowHeight);
		shadowsFBO->AddAttach(FrameBufferAttachmentFormat::Color_Attachment0, TextureType::Texture, ShadowMap.get());

#else

		ShadowMap->CreateEmptyTexture(TextureType::Texture, TextureDataType::DepthComponent, ShadowWidth, ShadowHeight, false);
		ShadowMap->SetRepeat(TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge, TextureRepeat::ClampToEdge);
		// See DirectionalLight::EnableCastShadows - Linear depth+compare is
		// unloadable on Apple GL; PCF is shader-side so Nearest is fine.
		ShadowMap->SetMinMagFilter(TextureFilter::Nearest, TextureFilter::Nearest);
		ShadowMap->EnableCompareMode();

		// Initialize Frame Buffer
		shadowsFBO->Init(FrameBufferAttachmentFormat::Depth_Attachment, TextureType::Texture, ShadowMap.get());

#endif

		// Near and Far Clip Planes
		ShadowNear = Near;
		ShadowFar = Radius;
	}

}