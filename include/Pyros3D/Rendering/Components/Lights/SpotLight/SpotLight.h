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

	struct SunShadowCache;
	class PYROS3D_API SpotLight : public ILightComponent {

	public:

		// The shadow cache (IRenderer's spot pass): what stands still in the cone drawn into a map of its
		// own once - again only when something of it arrives, leaves or moves, or the light does - and each
		// frame that map put in place and only what moves drawn over it. On unless turned off.
		void SetShadowCache(const bool on) { shadowCacheOn = on; }
		bool GetShadowCache() const { return shadowCacheOn; }
		bool shadowCacheOn = true;
		std::shared_ptr<SunShadowCache> shadowCache;      // (the renderer's)

		SpotLight() : ILightComponent(LIGHT_TYPE::SPOT) { Color = Vec4(1, 1, 1, 1); Radius = 1.f; }

		SpotLight(const Vec4 &color, const f32 radius, const Vec3 &direction, const f32 OutterCone, const f32 InnerCone);

		virtual ~SpotLight() {}

		virtual void Start() {};
		virtual void Update(const f64 time = 0) {};
		virtual void Destroy() {};
		virtual uint32 GetComponentType() const { return ComponentType::SpotLight; }
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

		// A lamp made to light the ground it stands over evenly - a car's headlamp, a street lamp's head: what it
		// throws downward grows weaker the steeper the ray, by the cube of the sine of its angle under the
		// horizontal past this one. A plain cone lights the ground by height / distance^3: white at the lamp's foot
		// and nothing a few metres on. With it the ground is lit alike from the lamp's foot out to where the ray at
		// this angle lands (height / tan), and as a plain cone's from there on; what stands up in the beam - a wall,
		// a car - is lit as before. The sine of the angle (0.05: a lamp 0.5 m up, even to 10 m); 0: a plain cone.
		void SetGroundEven(const f32 sine) { groundEven = sine < 0.f ? 0.f : (sine > 0.45f ? 0.45f : sine); }
		f32 GetGroundEven() const { return groundEven; }

	protected:

		// Light Direction
		Vec3 Direction;
		// Cone
		f32 outterCone, CosOutterCone, innerCone, CosInnerCone;
		// Attenuation
		f32 Radius;
		f32 groundEven = 0.f;

	};

}

#endif	/* SPOTLIGHT_H */