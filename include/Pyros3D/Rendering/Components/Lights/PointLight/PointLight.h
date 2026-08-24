//============================================================================
// Name        : PointLight.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Point Light
//============================================================================

#ifndef POINTLIGHT_H
#define	POINTLIGHT_H

#include <Pyros3D/Rendering/Components/Lights/ILightComponent.h>

namespace p3d {

	class PYROS3D_API PointLight : public ILightComponent {

	public:

		PointLight() : ILightComponent(LIGHT_TYPE::POINT) { Color = Vec4(1, 1, 1, 1); Radius = 1.f; ShadowBiasScale = 0.02f; }
		PointLight(const Vec4 &color, const f32 radius);
		virtual ~PointLight() {}

		virtual void Start() {};
		virtual void Update(const f64 time = 0) {};
		virtual void Destroy() {};

		virtual uint32 GetComponentType() const { return ComponentType::PointLight; }

		virtual const f32 &GetShadowFar() const
		{
			return Radius;
		}

		const f32 &GetLightRadius() const 
		{ 
			return Radius; 
		}

		void SetLightRadius(const f32 radius) 
		{
		   	Radius = radius; 
		}

		// How far toward the light a receiver is pulled before its depth is
		// compared against the shadow cube map, as a *fraction of its
		// distance to the light*.
		//
		// A point light needs its own knob because SetShadowBias()'s polygon
		// offset does nothing for it: the cube map is an R32F *colour*
		// attachment (see EnableCastShadows()), and polygon offset biases a
		// depth attachment only. That left the bias as a hardcoded constant
		// in two copies of PCFPOINT, unreachable from code, Lua or a scene.
		//
		// A fraction rather than a distance because the error this has to
		// cover is texel quantisation, and a cube face's texel footprint
		// grows linearly with distance: at N texels a side it is 2*d/N
		// across, so a constant fraction is a constant number of texels at
		// every distance. The constant it replaces was in *projected* depth,
		// where d(depth)/d(distance) is A*near/distance^2 - it bought 0.075
		// world units of slack at distance 5 and 4.8 at distance 40, which
		// is why it acned close to a light and over-detached far from one.
		// The 0.02 default is about 5 texels on a 512 cube.
		void SetShadowBiasScale(const f32 fractionOfDistance)
		{
			ShadowBiasScale = fractionOfDistance;
		}
		const f32 &GetShadowBiasScale() const
		{
			return ShadowBiasScale;
		}

		void EnableCastShadows(const uint32 Width, const uint32 Height, const f32 Near = 0.1f);

	protected:

		// Attenuation
		f32 Radius;
		// See SetShadowBiasScale().
		f32 ShadowBiasScale;

	};

}

#endif	/* POINTLIGHT_H */
