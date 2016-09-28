//============================================================================
// Name        : SpotLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Spot Light
//============================================================================

#ifndef SPOTLIGHT_H
#define	SPOTLIGHT_H

#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>

namespace p3d {

	class SpotLight : public ILightComponent {

	public:

		SpotLight() : ILightComponent(LIGHT_TYPE::SPOT) { Color = Vec4(1, 1, 1, 1); Radius = 1.f; }

		SpotLight(const Vec4 &color, const f32 radius, const Vec3 &direction, const f32 OutterCone, const f32 InnerCone) : ILightComponent(LIGHT_TYPE::SPOT)
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

		virtual ~SpotLight() {}

		virtual void Start() {};
		virtual void Update() {};
		virtual void Destroy() {};
		virtual const f32 &GetShadowFar() const
		{
			return Radius;
		}

		const Vec3 &GetLightDirection() const { return Direction; }
		void SetLightDirection(const Vec3 &direction) { Direction = direction; }
		const f32 &GetLightCosInnerCone() const { return CosInnerCone; }
		const f32 &GetLightCosOutterCone() const { return CosOutterCone; }
		const f32 &GetLightInnerCone() const { return innerCone; }
		const f32 &GetLightOutterCone() const { return outterCone; }
		void SetLightInnerCone(const f32 inner) { innerCone = inner; CosInnerCone = cosf((f32)DEGTORAD(innerCone)); }
		void SetLightOutterCone(const f32 outter) { outterCone = outter; CosOutterCone = cosf((f32)DEGTORAD(outterCone)); }
		const f32 &GetLightRadius() const { return Radius; }
		void SetLightRadius(const f32 radius) { Radius = radius; }

		void EnableCastShadows(const uint32 Width, const uint32 Height, const f32 Near = 0.1f)
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

	protected:

		// Light Direction
		Vec3 Direction;
		// Cone
		f32 outterCone, CosOutterCone, innerCone, CosInnerCone;
		// Attenuation
		f32 Radius;

	};

}

#endif	/* SPOTLIGHT_H */