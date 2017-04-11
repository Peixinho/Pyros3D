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

	class PYROS3D_API SpotLight : public ILightComponent {

	public:

		SpotLight() : ILightComponent(LIGHT_TYPE::SPOT) { Color = Vec4(1, 1, 1, 1); Radius = 1.f; }

		SpotLight(const Vec4 &color, const f32 radius, const Vec3 &direction, const f32 OutterCone, const f32 InnerCone);

		virtual ~SpotLight() {}

		virtual void Start() {};
		virtual void Update(const f64 time = 0) {};
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

		void EnableCastShadows(const uint32 Width, const uint32 Height, const f32 Near = 0.1f);

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